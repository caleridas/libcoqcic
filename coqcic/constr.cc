#include "coqcic/constr.h"

#include <stdexcept>

#include "coqcic/simpl.h"

#include <iostream>

namespace coqcic {

////////////////////////////////////////////////////////////////////////////////
// constr

void
constr::format(std::string& out) const
{
	repr_->format(out);
}

bool
constr::operator==(const constr& other) const
{
	return repr_ == other.repr_ || *repr_ == *other.repr_;
}

constr
constr::check(const type_context& ctx) const
{
	return repr_->check(ctx);
}

constr
constr::simpl() const
{
	return repr_->simpl();
}

constr
constr::shift(std::size_t limit, int dir) const
{
	return repr_->shift(limit, dir);
}

std::string
constr::debug_string() const
{
	std::string result;
	format(result);
	return result;
}

////////////////////////////////////////////////////////////////////////////////
// free functions on constr

static void
collect_external_references(const constr& obj, std::size_t depth, std::vector<std::size_t> refs)
{
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

// Collect the de Bruijn indices of all locals in this object that
// are not resolvable within the construct itself.
std::vector<std::size_t>
collect_external_references(const constr& obj)
{
	std::vector<std::size_t> refs;
	collect_external_references(obj, 0, refs);

	std::sort(refs.begin(), refs.end());
	refs.erase(std::unique(refs.begin(), refs.end()), refs.end());

	return refs;
}

////////////////////////////////////////////////////////////////////////////////
// type_context

type_context
type_context::push_local(std::string name, constr type) const
{
	type_context new_ctx(*this);
	new_ctx.locals = locals.push(local_entry{std::move(name), std::move(type)});
	return new_ctx;
}

////////////////////////////////////////////////////////////////////////////////
// constr_base

constr_base::~constr_base()
{
}

constr
constr_base::simpl() const
{
	return constr(shared_from_this());
}

constr
constr_base::shift(std::size_t limit, int dir) const
{
	return constr(shared_from_this());
}

std::string
constr_base::repr() const
{
	std::string result;
	format(result);
	return result;
}

////////////////////////////////////////////////////////////////////////////////
// constr_local

constr_local::~constr_local()
{
}

constr_local::constr_local(
	std::string name,
	std::size_t index)
	: name_(std::move(name))
	, index_(std::move(index))
{
}

void
constr_local::format(std::string& out) const
{
	out += name_ + "," + std::to_string(index_);
}

bool
constr_local::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_local = dynamic_cast<const constr_local*>(&other)) {
		return index_ == other_local->index_;
	} else {
		return false;
	}
}

constr
constr_local::check(const type_context& ctx) const
{
	return ctx.locals.at(index_).type;
}

constr
constr_local::shift(std::size_t limit, int dir) const
{
	if (index_ >= limit) {
		return constr(std::make_shared<constr_local>(name_, index_ + dir));
	} else {
		return constr(shared_from_this());
	}
}


////////////////////////////////////////////////////////////////////////////////
// constr_global

constr_global::~constr_global()
{
}

constr_global::constr_global(
	std::string name)
	: name_(std::move(name))
{
}

void
constr_global::format(std::string& out) const
{
	out += name_;
}

bool
constr_global::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_global = dynamic_cast<const constr_global*>(&other)) {
		return name_ == other_global->name_;
	} else {
		return false;
	}
}

constr
constr_global::check(const type_context& ctx) const
{
	return ctx.global_types(name_);
}

////////////////////////////////////////////////////////////////////////////////
// constr_builtin

constr_builtin::~constr_builtin()
{
}

constr_builtin::constr_builtin(
	std::string name,
	std::function<constr(const constr_base&)> check)
	: name_(std::move(name))
	, check_(std::move(check))
{
}

void
constr_builtin::format(std::string& out) const
{
	out += name_;
}

bool
constr_builtin::operator==(const constr_base& other) const noexcept
{
	return this == &other;
}

constr
constr_builtin::check(const type_context& ctx) const
{
	return check_(*this);
}

std::shared_ptr<const constr_base>
constr_builtin::get_set()
{
	static const std::shared_ptr<const constr_base> singleton =
		std::make_shared<constr_builtin>(
			"Set",
			[](const constr_base&) { return constr(get_type()); }
		);
	return singleton;
}

std::shared_ptr<const constr_base>
constr_builtin::get_prop()
{
	static const std::shared_ptr<const constr_base> singleton =
		std::make_shared<constr_builtin>(
			"Prop",
			[](const constr_base&) { return constr(get_type()); }
		);
	return singleton;
}

std::shared_ptr<const constr_base>
constr_builtin::get_sprop()
{
	static const std::shared_ptr<const constr_base> singleton =
		std::make_shared<constr_builtin>(
			"SProp",
			[](const constr_base&) { return constr(get_type()); }
		);
	return singleton;
}

std::shared_ptr<const constr_base>
constr_builtin::get_type()
{
	static const std::shared_ptr<const constr_base> singleton =
		std::make_shared<constr_builtin>(
			"Type",
			[](const constr_base&) { return constr(get_type()); }
		);
	return singleton;
}

////////////////////////////////////////////////////////////////////////////////
// constr_product

constr_product::~constr_product()
{
}

constr_product::constr_product(
	std::vector<formal_arg_t> args,
	constr restype)
	: args_(std::move(args))
	, restype_(std::move(restype))
{
}

void
constr_product::format(std::string& out) const
{
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
constr_product::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_product = dynamic_cast<const constr_product*>(&other)) {
		return args_ == other_product->args_ && restype_ == other_product->restype_;
	} else {
		return false;
	}
}

constr
constr_product::check(const type_context& ctx) const
{
	type_context new_ctx = ctx;
	std::optional<constr> expr_type;
	for (const auto& arg : args_) {
		auto t = arg.type.check(new_ctx);
		new_ctx = new_ctx.push_local(arg.name ? *arg.name : "_", arg.type);
		if (!expr_type) {
			expr_type = t;
		} else if (*expr_type != t) {
			expr_type = constr(constr_builtin::get_type());
		}
	}
	auto t = restype_.check(new_ctx);
	if (!expr_type) {
		expr_type = t;
	} else if (expr_type != t) {
		expr_type = constr(constr_builtin::get_type());
	}

	return *expr_type;
}

constr
constr_product::shift(std::size_t limit, int dir) const
{
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
		return constr(std::make_shared<constr_product>(std::move(args), std::move(restype)));
	} else {
		return constr(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_lambda

constr_lambda::~constr_lambda()
{
}

constr_lambda::constr_lambda(
	std::vector<formal_arg_t> args,
	constr body)
	: args_(std::move(args))
	, body_(std::move(body))
{
}

void
constr_lambda::format(std::string& out) const
{
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
constr_lambda::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_lambda = dynamic_cast<const constr_lambda*>(&other)) {
		return args_ == other_lambda->args_ && body_ == other_lambda->body_;
	} else {
		return false;
	}
}

constr
constr_lambda::check(const type_context& ctx) const
{
	type_context new_ctx = ctx;
	for (const auto& arg : args_) {
		type_context new_ctx = ctx.push_local(arg.name ? *arg.name : "_", arg.type);
	}
	auto restype = body_.check(new_ctx);
	return constr(std::make_shared<constr_product>(args(), std::move(restype)));
}

constr
constr_lambda::shift(std::size_t limit, int dir) const
{
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
		return constr(std::make_shared<constr_lambda>(std::move(args), std::move(body)));
	} else {
		return constr(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_let

constr_let::~constr_let()
{
}

constr_let::constr_let(std::optional<std::string> varname, constr value, constr body)
	: varname_(std::move(varname))
	, value_(std::move(value))
	, body_(std::move(body))
{
}

void
constr_let::format(std::string& out) const
{
	out += "let ";
	if (varname_) {
		out += *varname_;
	} else {
		out += "_";
	}
	out+= " := ";
	value_.format(out);
	out += " in (";
	body_.format(out);
	out += ')';
}

bool
constr_let::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_let = dynamic_cast<const constr_let*>(&other)) {
		return value_ == other_let->value_ && body_ == other_let->body_;
	} else {
		return false;
	}
}

constr
constr_let::check(const type_context& ctx) const
{
	type_context new_ctx = ctx.push_local(varname_ ? *varname_ : "_", value_.check(ctx));
	return body_.check(new_ctx);
}

constr
constr_let::shift(std::size_t limit, int dir) const
{
	auto value = value_.shift(limit, dir);
	auto body = body_.shift(limit + 1, dir);
	if (value.repr() != value_.repr() || body.repr() != body_.repr()) {
		return constr(std::make_shared<constr_let>(varname_, std::move(value), std::move(body)));
	} else {
		return constr(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_apply

constr_apply::~constr_apply()
{
}

constr_apply::constr_apply(constr fn, std::vector<constr> args)
	: fn_(std::move(fn))
	, args_(std::move(args))
{
}

void
constr_apply::format(std::string& out) const
{
	out += '(';
	fn_.format(out);
	for (const auto& arg : args()) {
		out += " ";
		arg.format(out);
	}
	out += ')';
}

bool
constr_apply::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_apply = dynamic_cast<const constr_apply*>(&other)) {
		return fn_ == other_apply->fn_ && args_ == other_apply->args_;
	} else {
		return false;
	}
}

constr
constr_apply::check(const type_context& ctx) const
{
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

	constr restype =
		residual_formal_args.empty() ?
		fntype : builder::product(std::move(residual_formal_args), fntype);

	std::vector<constr> subst;
	for (auto i = args_.rbegin(); i != args_.rend(); ++i) {
		subst.push_back(*i);
	}
	return local_subst(restype, 0, subst);
}

constr
constr_apply::simpl() const
{
	if (auto fnlambda = dynamic_cast<const constr_lambda*>(fn_.repr().get())) {
		std::size_t nsubst = std::min(args().size(), fnlambda->args().size());
		std::vector<formal_arg_t> residual_formal_args(fnlambda->args().begin() + nsubst, fnlambda->args().end());
		constr resfn =
			residual_formal_args.empty() ?
			fnlambda->body() : builder::lambda(std::move(residual_formal_args), fnlambda->body());

		std::vector<constr> subst;
		for (std::size_t n = 0; n < nsubst; ++n) {
			subst.push_back(args()[nsubst - n - 1]);
		}
		constr res = local_subst(resfn, 0, subst);
		if (nsubst != args().size()) {
			res = builder::apply(std::move(res), {args().begin() + nsubst, args().end()});
		}
		return res.simpl();
	} else {
		return constr(shared_from_this());
	}
}

constr
constr_apply::shift(std::size_t limit, int dir) const
{
	auto fn = fn_.shift(limit, dir);
	bool change = fn.repr() != fn_.repr();

	std::vector<constr> args;

	for (const auto& arg : args_) {
		auto new_arg = arg.shift(limit, dir);
		change = change || new_arg.repr() != arg.repr();
		args.push_back(std::move(new_arg));
	}

	if (change) {
		return constr(std::make_shared<constr_apply>(std::move(fn), std::move(args)));
	} else {
		return constr(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_cast

constr_cast::~constr_cast()
{
}

constr_cast::constr_cast(constr term, kind_type kind, constr typeterm)
	: term_(std::move(term)), kind_(kind), typeterm_(std::move(typeterm))
{
}

void
constr_cast::format(std::string& out) const
{
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
constr_cast::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_cast = dynamic_cast<const constr_cast*>(&other)) {
		return term_ == other_cast->term_ && kind_ == other_cast->kind_ && typeterm_ == other_cast->typeterm_;
	} else {
		return false;
	}
}

constr
constr_cast::check(const type_context& ctx) const
{
	return term_.check(ctx);
}

constr
constr_cast::shift(std::size_t limit, int dir) const
{
	auto term = term_.shift(limit, dir);
	auto typeterm = typeterm_.shift(limit, dir);
	if (term.repr() != term_.repr() || typeterm.repr() != typeterm_.repr()) {
		return constr(std::make_shared<constr_cast>(std::move(term), kind_, std::move(typeterm)));
	} else {
		return constr(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_match

constr_match::~constr_match()
{
}

constr_match::constr_match(constr restype, constr arg, std::vector<match_branch_t> branches)
	: restype_(std::move(restype))
	, arg_(std::move(arg))
	, branches_(std::move(branches))
{
}

void
constr_match::format(std::string& out) const
{
	out += "match ";
	arg_.format(out);
	out += " restype ";
	restype_.format(out);
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
constr_match::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_match = dynamic_cast<const constr_match*>(&other)) {
		return restype_ == other_match->restype_ && arg_ == other_match->arg_ && branches_ == other_match->branches_;
	} else {
		return false;
	}
}

constr
constr_match::check(const type_context& ctx) const
{
	auto argtype = arg_.check(ctx);
	return local_subst(restype_, 0, {argtype});
}

constr
constr_match::shift(std::size_t limit, int dir) const
{
	bool diff = false;

	auto restype = restype_.shift(limit + 1, dir);
	diff = diff || restype.repr() != restype_.repr();
	auto arg = arg_.shift(limit, dir);
	diff = diff || arg.repr() != arg_.repr();

	std::vector<match_branch_t> branches;
	for (const auto& b : branches_) {
		auto expr = b.expr.shift(limit, dir);
		diff = diff || expr.repr() != b.expr.repr();
		branches.push_back(match_branch_t { b.constructor, b.nargs, expr });
	}

	if (diff) {
		return constr(std::make_shared<constr_match>(std::move(restype), std::move(arg), std::move(branches)));
	} else {
		return constr(shared_from_this());
	}
}

////////////////////////////////////////////////////////////////////////////////
// constr_fix

constr_fix::~constr_fix()
{
}

constr_fix::constr_fix(std::size_t index, std::shared_ptr<const fix_group_t> group)
	: index_(index), group_(std::move(group))
{
}

void
constr_fix::format(std::string& out) const
{
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
constr_fix::operator==(const constr_base& other) const noexcept
{
	if (this == &other) {
		return true;
	} else if (auto other_fix = dynamic_cast<const constr_fix*>(&other)) {
		return group_ == other_fix->group_ && index_ == other_fix->index_;
	} else {
		return false;
	}
}

constr
constr_fix::check(const type_context& ctx) const
{
	const auto& fn = group_->functions[index_];
	return builder::product(fn.args, fn.restype);
}

constr
constr_fix::shift(std::size_t limit, int dir) const
{
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
		return constr(std::make_shared<constr_fix>(index_, std::move(group)));
	} else {
		return constr(shared_from_this());
	}
}

namespace builder {

constr
local(std::string name, std::size_t index)
{
	return constr(std::make_shared<constr_local>(std::move(name), std::move(index)));
}

constr
global(std::string name)
{
	return constr(std::make_shared<constr_global>(std::move(name)));
}

constr
builtin_set()
{
	return constr(constr_builtin::get_set());
}

constr
builtin_prop()
{
	return constr(constr_builtin::get_prop());
}

constr
builtin_sprop()
{
	return constr(constr_builtin::get_sprop());
}

constr
builtin_type()
{
	return constr(constr_builtin::get_type());
}

constr
product(std::vector<formal_arg_t> args, constr restype)
{
	return constr(std::make_shared<constr_product>(std::move(args), std::move(restype)));
}

constr
lambda(std::vector<formal_arg_t> args, constr body)
{
	return constr(std::make_shared<constr_lambda>(std::move(args), std::move(body)));
}

constr
let(std::optional<std::string> varname, constr value, constr body)
{
	return constr(std::make_shared<constr_let>(std::move(varname), std::move(value), std::move(body)));
}

constr
apply(constr fn, std::vector<constr> args)
{
	return constr(std::make_shared<constr_apply>(std::move(fn), std::move(args)));
}

constr
cast(constr term, constr_cast::kind_type kind, constr typeterm)
{
	return constr(std::make_shared<constr_cast>(std::move(term), kind, std::move(typeterm)));
}


constr
match(constr restype, constr arg, std::vector<match_branch_t> branches)
{
	return constr(std::make_shared<constr_match>(std::move(restype), std::move(arg), std::move(branches)));
}

constr
fix(std::size_t index, std::shared_ptr<const fix_group_t> group)
{
	return constr(std::make_shared<constr_fix>(index, std::move(group)));
}

}  // builder

}  // namespace coqcic
