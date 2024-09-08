#include "coqcic/constr.h"

#include <stdexcept>

#include "coqcic/simpl.h"

#include <iostream>

namespace coqcic {

////////////////////////////////////////////////////////////////////////////////
// constr

/**
	\class constr_t
	\brief A coqcic term construction
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a term construct in the Coq calculus of
	inductive constructions.
*/

/**
	\fn constr_t constr_t::operator=(constr_t other)
	\brief Assign constr
*/

/**
	\fn constr_t::swap
	\brief Swaps two constr
*/

/**
	\brief Generates human-readable representation

	\param out
		String to append representation to
*/
void
constr_t::format(std::string& out) const {
	repr_->format(out);
}

/**
	\brief Compares for equality with other term
	\param other
		Term to compare to
	\returns
		Equality comparison result

	Compares two term constructions for equality. Note
	that this checks for strict structural equality (i.e.
	grouping of nested lambda/product is significant), but
	does not check for names of local variables (just their
	de Bruijn indices).
*/
bool
constr_t::operator==(const constr_t& other) const {
	return repr_ == other.repr_ || *repr_ == *other.repr_;
}

/**
	\brief Checks type of constr
	\param ctx
		Typing context.

	\returns
		Expression representing type of object.

	Computes an expression for the type of this object,
	within the given typing context. The typing context
	must be able to resolve all global (and ubound local)
	variables.
*/
constr_t
constr_t::check(const type_context_t& ctx) const {
	return repr_->check(ctx);
}

/**
	\brief Simplifies term.

	\returns
		Simplified term.

	Resolves apply / lambda pairs to produce a simplified term.
*/
constr_t
constr_t::simpl() const {
	return repr_->simpl();
}

/**
	\brief Shifts de Bruijn indices.

	\param limit
		Lower limit of de Bruijn indices to be shifted. All indices
		>= limit will be shifted.
	\param dir
		Offset to be added to de Bruijn indices.

	\returns
		Modified term.

	Shifts (subset of) de Bruijn indices occuring in term,
	recursively. All indices >=limit (as viewed from root)
	will be modified by the given offset. Note that this
	can only ever affect "unbound" indices.

	Example: Consider the term:

		lambda (x : nat) : nat := '0 + '1

	The body of the lambda expression has two local
	variable references: '0 refers to its defined
	formal argument, while '1 refers to the zeroe'th
	object outside this term. Calling "shift(0, +1)"
	results in:

		lambda (x : nat) : nat := '0 + '2

	Calling "shift(1, +1)" results in:

		lambda (x : nat) : nat := '0 + '1

	The latter operation does not cause a change because '1 inside
	the lambda abstraction refers to the zeroe'th unbound variable
	for the term as a whole.
*/

constr_t
constr_t::shift(std::size_t limit, int dir) const {
	return repr_->shift(limit, dir);
}

/**
	\brief Generates a debug string.

	\returns Human-readable string for diagnostic purposes.

	Generates a string that may be useful in understanding the
	structure of a term construction for error diagnostics.
*/
std::string
constr_t::debug_string() const {
	std::string result;
	format(result);
	return result;
}

/**
	\fn constr_t::visit
	\brief Discriminates kind of constr

	\param vis
		Visitor functional. Should be an overloaded / type generic lambda.
	\returns
		Value returned by the visitor

	Determines the underlying kind of constr represented by this
	object, and dispatches to the given visitor function with the
	specific type of constr. See
		\ref constr_local,
		\ref constr_global,
		\ref constr_builtin,
		\ref constr_product,
		\ref constr_lambda,
		\ref constr_let,
		\ref constr_apply,
		\ref constr_cast,
		\ref constr_match,
		\ref constr_fix.

	Canonically, the visitor should have the following structure:
	\code
		constr.visit(
			[](const auto& const) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same<T, coqcic::constr_local>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_global>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_builtin>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_product>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_lambda>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_let>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_apply>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_cast>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_match>()) {
					...
				} else if constexpr (std::is_same<T, coqcic::constr_fix>()) {
					...
				} else {
					throw std::logic_error("non-exhaustice pattern matching on constr_t");
				}
			}
		);
	\endcode
*/

////////////////////////////////////////////////////////////////////////////////
// free functions on constr

static void
collect_external_references(const constr_t& obj, std::size_t depth, std::vector<std::size_t> refs) {
	if (auto local = obj.as_local()) {
		if (local->index() >= depth) {
			refs.push_back(local->index() - depth);
		}
	} else if (auto product = obj.as_product()) {
		for (const auto& arg : product->args()) {
			collect_external_references(arg.type, depth, refs);
			++depth;
		}
		collect_external_references(product->restype(), depth , refs);
	} else if (auto lambda = obj.as_lambda()) {
		for (const auto& arg : product->args()) {
			collect_external_references(arg.type, depth, refs);
			++depth;
		}
		collect_external_references(lambda->body(), depth + 1, refs);
	} else if (auto let = obj.as_let()) {
		collect_external_references(let->value(), depth, refs);
		collect_external_references(let->type(), depth, refs);
		collect_external_references(let->body(), depth + 1, refs);
	} else if (auto fix = obj.as_fix()) {
		depth += fix->group()->functions.size();
		for (const auto& fn : fix->group()->functions) {
			std::size_t current_depth = depth;
			for (const auto& arg : fn.args) {
				collect_external_references(arg.type, current_depth, refs);
				current_depth += 1;
			}
			collect_external_references(fn.restype, current_depth, refs);
			collect_external_references(fn.body, current_depth, refs);
		}
	}
}

/**
	\brief Collects unbound de Bruijn indices.

	\param obj
		Term to collect indices for.

	\returns
		Collection of unbound de Bruijn indices.

	Returns all de Bruijn indices of local variable references
	that cannot be resolved within the present term. This is equivalent
	to specifying to "context stack" providing local variables that
	would be required to evaluate this expression.

	Example: Consider the term:

		lambda (x : nat) : nat := '0 + '1

	This would result in the following set of indices collected:

		[0]

	This represents the use of variable '1 which has index 1
	inside the bound lambda, referring to index 0 for the term
	as a whole.
*/
std::vector<std::size_t>
collect_external_references(const constr_t& obj) {
	std::vector<std::size_t> refs;
	collect_external_references(obj, 0, refs);

	std::sort(refs.begin(), refs.end());
	refs.erase(std::unique(refs.begin(), refs.end()), refs.end());

	return refs;
}


////////////////////////////////////////////////////////////////////////////////
// fix_group_t

constr_t
fix_group_t::get_function_signature(std::size_t index) const {
	std::vector<formal_arg_t> formargs;
	for (const auto& formarg : functions[index].args) {
		formargs.push_back(
			formal_arg_t {
				formarg.name,
				formarg.type.shift(0, -functions.size())
			}
		);
	}
	auto restype = functions[index].restype.shift(0, -functions.size());
	return builder::product(std::move(formargs), std::move(restype));
}

////////////////////////////////////////////////////////////////////////////////
// type_context_t

/**
	\class type_context_t
	\brief Typing context for type check operations
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents the context used in resolving the types of terms.
	It needs to provide a way of resolving types of all referenced
	global objects as well as all unbound locals.

	Type resolution is done recursively, as needed. When resolving
	products, lambdas etc that define new local variables, the
	type context used in resolution of sub terms will be extended
	accordingly.
*/

/**
	\brief Creates expanded type context.
	\param name
		Name of local variable
	\param type
		Type of local variable
	\returns
		New type context with additional local variable.

	Adds a new local variable to the type context, returns the
	extended type context. This is used when recursively resolving
	the types of sub-expressions for lambda, product etc constructions.
*/
type_context_t
type_context_t::push_local(std::string name, constr_t type) const {
	type_context_t new_ctx(*this);
	new_ctx.locals = locals.push(local_entry{std::move(name), std::move(type)});
	return new_ctx;
}

////////////////////////////////////////////////////////////////////////////////
// constr_base

/**
	\class constr_base
	\brief Abstract base class for term constructions.
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Defines the base class for all constr representation subclasses.
	Generally, this class is not used directly, refer to
	\ref constr_t::visit and similar to discriminate the different
	kinds of constrs.
*/
constr_base::~constr_base() {
}

constr_t
constr_base::simpl() const {
	return constr_t(shared_from_this());
}

constr_t
constr_base::shift(std::size_t limit, int dir) const {
	return constr_t(shared_from_this());
}

std::string
constr_base::repr() const {
	std::string result;
	format(result);
	return result;
}

////////////////////////////////////////////////////////////////////////////////
// constr_local

/**
	\class constr_local
	\brief Local variable reference
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a reference to an externally defined local variable
	from a (nested) term. Certain constructs (e.g. lambda, product)
	define formal arguments which can be referenced as a local
	variable.

	Each local is characterized is characterized by its "nesting depth",
	also called "de Bruijn index".
*/

constr_local::~constr_local() {
}

constr_local::constr_local(
	std::string name,
	std::size_t index
) : name_(std::move(name)),
	index_(std::move(index)) {
}

void
constr_local::format(std::string& out) const {
	out += name_ + "," + std::to_string(index_);
}

bool
constr_local::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_local = dynamic_cast<const constr_local*>(&other)) {
		return index_ == other_local->index_;
	} else {
		return false;
	}
}

constr_t
constr_local::check(const type_context_t& ctx) const {
	return ctx.locals.at(index_).type;
}

constr_t
constr_local::shift(std::size_t limit, int dir) const {
	if (index_ >= limit) {
		return constr_t(std::make_shared<constr_local>(name_, index_ + dir));
	} else {
		return constr_t(shared_from_this());
	}
}


////////////////////////////////////////////////////////////////////////////////
// constr_global

/**
	\class constr_global
	\brief Global object reference
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a reference to a global object. The reference is always made
	be the (fully qualified) name in string representation.
*/

constr_global::~constr_global() {
}

constr_global::constr_global(std::string name) : name_(std::move(name)) {
}

void
constr_global::format(std::string& out) const {
	out += name_;
}

bool
constr_global::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_global = dynamic_cast<const constr_global*>(&other)) {
		return name_ == other_global->name_;
	} else {
		return false;
	}
}

constr_t
constr_global::check(const type_context_t& ctx) const {
	return ctx.global_types(name_);
}

////////////////////////////////////////////////////////////////////////////////
// constr_builtin

/**
	\class constr_builtin
	\brief Local variable reference
	\headerfile coqcic/constr.h <coqcic/constr.h>

	A builtin sort of the Coq calculus of inductive constructions.
	These are <tt>Set</tt>, <tt>Prop</tt> and <tt>Type</tt> sorts.
*/

constr_builtin::~constr_builtin() {
}

constr_builtin::constr_builtin(
	std::string name,
	std::function<constr_t(const constr_base&)> check
) : name_(std::move(name)),
	check_(std::move(check)) {
}

void
constr_builtin::format(std::string& out) const {
	out += name_;
}

bool
constr_builtin::operator==(const constr_base& other) const noexcept {
	return this == &other;
}

constr_t
constr_builtin::check(const type_context_t& ctx) const {
	return check_(*this);
}

std::shared_ptr<const constr_base>
constr_builtin::get_set() {
	static const std::shared_ptr<const constr_base> singleton = std::make_shared<constr_builtin>(
		"Set",
		[](const constr_base&) { return constr_t(get_type()); }
	);
	return singleton;
}

std::shared_ptr<const constr_base>
constr_builtin::get_prop() {
	static const std::shared_ptr<const constr_base> singleton = std::make_shared<constr_builtin>(
		"Prop",
		[](const constr_base&) { return constr_t(get_type()); }
	);
	return singleton;
}

std::shared_ptr<const constr_base>
constr_builtin::get_sprop() {
	static const std::shared_ptr<const constr_base> singleton = std::make_shared<constr_builtin>(
		"SProp",
		[](const constr_base&) { return constr_t(get_type()); }
	);
	return singleton;
}

std::shared_ptr<const constr_base>
constr_builtin::get_type() {
	static const std::shared_ptr<const constr_base> singleton = std::make_shared<constr_builtin>(
		"Type",
		[](const constr_base&) { return constr_t(get_type()); }
	);
	return singleton;
}

////////////////////////////////////////////////////////////////////////////////
// constr_product

/**
	\class constr_product
	\brief Dependent product
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a dependent product. This is the type of a lambda abstraction
	with a number of formal arguments and specifies the computation of
	the result type of the lambda expression.
*/

constr_product::~constr_product() {
}

constr_product::constr_product(
	std::vector<formal_arg_t> args,
	constr_t restype
) : args_(std::move(args)), restype_(std::move(restype)) {
}

void
constr_product::format(std::string& out) const {
	out += '(';
	for (const auto& arg : args_) {
		if (arg.name) {
			out += *arg.name;
			out += " : ";
		}
		arg.type.format(out);
		out += " -> ";
	}
	restype_.format(out);
	out += ')';
}

bool
constr_product::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_product = dynamic_cast<const constr_product*>(&other)) {
		return args_ == other_product->args_ && restype_ == other_product->restype_;
	} else {
		return false;
	}
}

constr_t
constr_product::check(const type_context_t& ctx) const {
	type_context_t new_ctx = ctx;
	std::optional<constr_t> expr_type;
	for (const auto& arg : args_) {
		auto t = arg.type.check(new_ctx);
		new_ctx = new_ctx.push_local(arg.name ? *arg.name : "_", arg.type);
		if (!expr_type) {
			expr_type = t;
		} else if (*expr_type != t) {
			expr_type = constr_t(constr_builtin::get_type());
		}
	}
	auto t = restype_.check(new_ctx);
	if (!expr_type) {
		expr_type = t;
	} else if (expr_type != t) {
		expr_type = constr_t(constr_builtin::get_type());
	}

	return *expr_type;
}

constr_t
constr_product::shift(std::size_t limit, int dir) const {
	std::vector<formal_arg_t> args;
	bool change = false;

	for (const auto& arg : args_) {
		auto new_arg_type = arg.type.shift(limit, dir);
		++limit;
		args.push_back({arg.name, new_arg_type});
		change = change || new_arg_type.repr() != arg.type.repr();
	}

	auto restype = restype_.shift(limit, dir);
	change = change || restype.repr() != restype_.repr();

	if (change) {
		return constr_t(std::make_shared<constr_product>(std::move(args), std::move(restype)));
	} else {
		return constr_t(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_lambda

/**
	\class constr_lambda
	\brief Lambda abstraction
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a lambda abstraction, i.e. a rule that describes how to
	compute a value from specified formal arguments.
*/


constr_lambda::~constr_lambda() {
}

constr_lambda::constr_lambda(
	std::vector<formal_arg_t> args,
	constr_t body
) : args_(std::move(args)), body_(std::move(body)) {
}

void
constr_lambda::format(std::string& out) const {
	out += '(';
	for (const auto& arg : args_) {
		if (arg.name) {
			out += *arg.name;
			out += " : ";
		}
		arg.type.format(out);
		out += " => ";
	}
	body_.format(out);
	out += ')';
}

bool
constr_lambda::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_lambda = dynamic_cast<const constr_lambda*>(&other)) {
		return args_ == other_lambda->args_ && body_ == other_lambda->body_;
	} else {
		return false;
	}
}

constr_t
constr_lambda::check(const type_context_t& ctx) const {
	type_context_t new_ctx = ctx;
	for (const auto& arg : args_) {
		type_context_t new_ctx = ctx.push_local(arg.name ? *arg.name : "_", arg.type);
	}
	auto restype = body_.check(new_ctx);
	return constr_t(std::make_shared<constr_product>(args(), std::move(restype)));
}

constr_t
constr_lambda::shift(std::size_t limit, int dir) const {
	std::vector<formal_arg_t> args;
	bool change = false;

	for (const auto& arg : args_) {
		auto new_arg_type = arg.type.shift(limit, dir);
		++limit;
		args.push_back({arg.name, new_arg_type});
		change = change || new_arg_type.repr() != arg.type.repr();
	}

	auto body = body_.shift(limit, dir);
	change = change || body.repr() != body_.repr();

	if (change) {
		return constr_t(std::make_shared<constr_lambda>(std::move(args), std::move(body)));
	} else {
		return constr_t(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_let

/**
	\class constr_let
	\brief Let binding
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a let binding: An expression is bound to a local variable which
	can be referenced (possibly multiple times) in a sub expression.
*/

constr_let::~constr_let() {
}

constr_let::constr_let(
	std::optional<std::string> varname,
	constr_t value,
	constr_t type,
	constr_t body
) : varname_(std::move(varname)),
	value_(std::move(value)),
	type_(std::move(type)),
	body_(std::move(body)) {
}

void
constr_let::format(std::string& out) const {
	out += "let ";
	if (varname_) {
		out += *varname_;
	} else {
		out += "_";
	}
	out += " : ";
	type_.format(out);
	out += " := ";
	value_.format(out);
	out += " in (";
	body_.format(out);
	out += ')';
}

bool
constr_let::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_let = dynamic_cast<const constr_let*>(&other)) {
		return value_ == other_let->value_ && type_ == other_let->type_ && body_ == other_let->body_;
	} else {
		return false;
	}
}

constr_t
constr_let::check(const type_context_t& ctx) const {
	type_context_t new_ctx = ctx.push_local(varname_ ? *varname_ : "_", value_.check(ctx));
	return body_.check(new_ctx);
}

constr_t
constr_let::shift(std::size_t limit, int dir) const {
	auto value = value_.shift(limit, dir);
	auto type = type_.shift(limit, dir);
	auto body = body_.shift(limit + 1, dir);
	if (value.repr() != value_.repr() || type.repr() != type_.repr() || body.repr() != body_.repr()) {
		return constr_t(std::make_shared<constr_let>(varname_, std::move(value), std::move(type), std::move(body)));
	} else {
		return constr_t(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_apply

/**
	\class constr_apply
	\brief Functional application
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents functional application of a (function) term to
	actual arguments.
*/

constr_apply::~constr_apply() {
}

constr_apply::constr_apply(
	constr_t fn,
	std::vector<constr_t> args
) : fn_(std::move(fn)) , args_(std::move(args)) {
}

void
constr_apply::format(std::string& out) const {
	out += '(';
	fn_.format(out);
	for (const auto& arg : args()) {
		out += " ";
		arg.format(out);
	}
	out += ')';
}

bool
constr_apply::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_apply = dynamic_cast<const constr_apply*>(&other)) {
		return fn_ == other_apply->fn_ && args_ == other_apply->args_;
	} else {
		return false;
	}
}

constr_t
constr_apply::check(const type_context_t& ctx) const {
	auto fntype = fn_.check(ctx);
	std::vector<formal_arg_t> prod_args;

	for (;;) {
		auto product = fntype.as_product();
		if (!product) break;
		prod_args.insert(prod_args.end(), product->args().begin(), product->args().end());
		fntype = product->restype();
	}

	if (prod_args.size() < args().size()) {
		throw std::runtime_error("Functional application to non-product");
	}

	std::size_t nsubst = args().size();

	std::vector<formal_arg_t> residual_formal_args(prod_args.begin() + nsubst, prod_args.end());

	constr_t restype =
		residual_formal_args.empty() ?
		fntype : builder::product(std::move(residual_formal_args), fntype);

	std::vector<constr_t> subst;
	for (auto i = args_.rbegin(); i != args_.rend(); ++i) {
		subst.push_back(*i);
	}
	return local_subst(restype, 0, subst);
}

constr_t
constr_apply::simpl() const {
	if (auto fnlambda = dynamic_cast<const constr_lambda*>(fn_.repr().get())) {
		std::size_t nsubst = std::min(args().size(), fnlambda->args().size());
		std::vector<formal_arg_t> residual_formal_args(fnlambda->args().begin() + nsubst, fnlambda->args().end());
		constr_t resfn =
			residual_formal_args.empty() ?
			fnlambda->body() : builder::lambda(std::move(residual_formal_args), fnlambda->body());

		std::vector<constr_t> subst;
		for (std::size_t n = 0; n < nsubst; ++n) {
			subst.push_back(args()[nsubst - n - 1]);
		}
		constr_t res = local_subst(resfn, 0, subst);
		if (nsubst != args().size()) {
			res = builder::apply(std::move(res), {args().begin() + nsubst, args().end()});
		}
		return res.simpl();
	} else {
		return constr_t(shared_from_this());
	}
}

constr_t
constr_apply::shift(std::size_t limit, int dir) const {
	auto fn = fn_.shift(limit, dir);
	bool change = fn.repr() != fn_.repr();

	std::vector<constr_t> args;

	for (const auto& arg : args_) {
		auto new_arg = arg.shift(limit, dir);
		change = change || new_arg.repr() != arg.repr();
		args.push_back(std::move(new_arg));
	}

	if (change) {
		return constr_t(std::make_shared<constr_apply>(std::move(fn), std::move(args)));
	} else {
		return constr_t(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_cast

/**
	\class constr_cast
	\brief Cast expression
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a cast expression.
*/


constr_cast::~constr_cast() {
}

constr_cast::constr_cast(
	constr_t term,
	kind_type kind,
	constr_t typeterm
) : term_(std::move(term)), kind_(kind), typeterm_(std::move(typeterm)) {
}

void
constr_cast::format(std::string& out) const {
	out += "Cast(";
	term_.format(out);
	out += ",";
	switch (kind_) {
		case vm_cast: {
			out += "VMcast";
			break;
		}
		case default_cast: {
			out += "DEFAULTcast";
			break;
		}
		case revert_cast: {
			out += "REVERTcast";
			break;
		}
		case native_cast: {
			out += "NATIVEcast";
			break;
		}
	}
	out += ",";
	typeterm_.format(out);
	out += ")";
}

bool
constr_cast::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_cast = dynamic_cast<const constr_cast*>(&other)) {
		return term_ == other_cast->term_ && kind_ == other_cast->kind_ && typeterm_ == other_cast->typeterm_;
	} else {
		return false;
	}
}

constr_t
constr_cast::check(const type_context_t& ctx) const {
	return term_.check(ctx);
}

constr_t
constr_cast::shift(std::size_t limit, int dir) const {
	auto term = term_.shift(limit, dir);
	auto typeterm = typeterm_.shift(limit, dir);
	if (term.repr() != term_.repr() || typeterm.repr() != typeterm_.repr()) {
		return constr_t(std::make_shared<constr_cast>(std::move(term), kind_, std::move(typeterm)));
	} else {
		return constr_t(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_match

/**
	\class constr_match
	\brief Pattern matching expression
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents a pattern matching expression where an instance of an
	inductive type is subjected to (exhaustive) case analysis of
	all constructors of the underlying inductive type.
*/

constr_match::~constr_match() {
}

constr_match::constr_match(
	constr_t casetype,
	constr_t arg,
	std::vector<match_branch_t> branches
) : casetype_(std::move(casetype)), arg_(std::move(arg)), branches_(std::move(branches)) {
}

void
constr_match::format(std::string& out) const {
	out += "match ";
	arg_.format(out);
	out += " casetype ";
	casetype_.format(out);
	for (const auto& branch : branches_) {
		out += "| ";
		out += branch.constructor;
		out += " ";
		out += std::to_string(branch.nargs);
		out += " => ";
		branch.expr.format(out);
	}
	out += " end";
}

bool
constr_match::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_match = dynamic_cast<const constr_match*>(&other)) {
		return casetype_ == other_match->casetype_ && arg_ == other_match->arg_ && branches_ == other_match->branches_;
	} else {
		return false;
	}
}

constr_t
constr_match::check(const type_context_t& ctx) const {
	auto argtype = arg_.check(ctx);
	return local_subst(casetype_, 0, {argtype});
}

constr_t
constr_match::shift(std::size_t limit, int dir) const {
	bool diff = false;

	auto casetype = casetype_.shift(limit , dir);
	diff = diff || casetype.repr() != casetype_.repr();
	auto arg = arg_.shift(limit, dir);
	diff = diff || arg.repr() != arg_.repr();

	std::vector<match_branch_t> branches;
	for (const auto& b : branches_) {
		auto expr = b.expr.shift(limit, dir);
		diff = diff || expr.repr() != b.expr.repr();
		branches.push_back(match_branch_t { b.constructor, b.nargs, expr });
	}

	if (diff) {
		return constr_t(std::make_shared<constr_match>(std::move(casetype), std::move(arg), std::move(branches)));
	} else {
		return constr_t(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_fix

/**
	\class constr_fix
	\brief Fixpoint function
	\headerfile coqcic/constr.h <coqcic/constr.h>

	Represents an entry point into a bundle of mutually recursive functions.
*/

constr_fix::~constr_fix() {
}

constr_fix::constr_fix(
	std::size_t index,
	std::shared_ptr<const fix_group_t> group
) : index_(index), group_(std::move(group)) {
}

void
constr_fix::format(std::string& out) const {
	out += "(fix ";
	bool first = true;
	for (const auto& function : group_->functions) {
		if (!first) {
			out += "with ";
		}
		first = false;
		out += function.name + " ";
		for (const auto& arg : function.args) {
			out += "(";
			out += arg.name ? *arg.name : "_";
			out += " : ";
			arg.type.format(out);
			out += ") ";
		}
		out += ": ";
		function.restype.format(out);
		out += " := ";
		function.body.format(out);
		out += " ";
	}
	out += "for ";
	out += group_->functions[index_].name;
	out += ")";
}

bool
constr_fix::operator==(const constr_base& other) const noexcept {
	if (this == &other) {
		return true;
	} else if (auto other_fix = dynamic_cast<const constr_fix*>(&other)) {
		return group_ == other_fix->group_ && index_ == other_fix->index_;
	} else {
		return false;
	}
}

constr_t
constr_fix::check(const type_context_t& ctx) const {
	const auto& fn = group_->functions[index_];
	return builder::product(fn.args, fn.restype);
}

constr_t
constr_fix::shift(std::size_t limit, int dir) const {
	bool changed = false;
	fix_group_t new_group;
	limit += group_->functions.size();
	for (const auto& fn : group_->functions) {
		fix_function_t new_fn;
		std::size_t add_index = 0;
		for (const auto& arg : fn.args) {
			auto new_arg = arg.type.shift(limit + add_index, dir);
			++add_index;
			changed = changed || (new_arg.repr() != arg.type.repr());
			new_fn.args.push_back({arg.name, new_arg});
		}
		new_fn.restype = fn.restype.shift(limit + add_index, dir);
		changed = changed || (new_fn.restype != fn.restype);
		new_fn.body = fn.body.shift(limit + add_index, dir);
		changed = changed || (new_fn.body != fn.body);
		new_group.functions.push_back(std::move(new_fn));
	}

	if (changed) {
		std::shared_ptr<const fix_group_t> group = std::make_shared<fix_group_t>(std::move(new_group));
		return constr_t(std::make_shared<constr_fix>(index_, std::move(group)));
	} else {
		return constr_t(shared_from_this());
	}
}

namespace builder {

constr_t
local(std::string name, std::size_t index) {
	return constr_t(std::make_shared<constr_local>(std::move(name), std::move(index)));
}

constr_t
global(std::string name) {
	return constr_t(std::make_shared<constr_global>(std::move(name)));
}

constr_t
builtin_set() {
	return constr_t(constr_builtin::get_set());
}

constr_t
builtin_prop() {
	return constr_t(constr_builtin::get_prop());
}

constr_t
builtin_sprop() {
	return constr_t(constr_builtin::get_sprop());
}

constr_t
builtin_type() {
	return constr_t(constr_builtin::get_type());
}

constr_t
product(std::vector<formal_arg_t> args, constr_t restype) {
	return constr_t(std::make_shared<constr_product>(std::move(args), std::move(restype)));
}

constr_t
lambda(std::vector<formal_arg_t> args, constr_t body) {
	return constr_t(std::make_shared<constr_lambda>(std::move(args), std::move(body)));
}

constr_t
let(
	std::optional<std::string> varname,
	constr_t value,
	constr_t type,
	constr_t body
) {
	return constr_t(std::make_shared<constr_let>(std::move(varname), std::move(value), std::move(type), std::move(body)));
}

constr_t
apply(constr_t fn, std::vector<constr_t> args) {
	return constr_t(std::make_shared<constr_apply>(std::move(fn), std::move(args)));
}

constr_t
cast(constr_t term, constr_cast::kind_type kind, constr_t typeterm) {
	return constr_t(std::make_shared<constr_cast>(std::move(term), kind, std::move(typeterm)));
}


constr_t
match(constr_t restype, constr_t arg, std::vector<match_branch_t> branches) {
	return constr_t(std::make_shared<constr_match>(std::move(restype), std::move(arg), std::move(branches)));
}

constr_t
fix(std::size_t index, std::shared_ptr<const fix_group_t> group) {
	return constr_t(std::make_shared<constr_fix>(index, std::move(group)));
}

}  // builder

}  // namespace coqcic
