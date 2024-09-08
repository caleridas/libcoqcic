#ifndef COQCIC_CONSTR_H
#define COQCIC_CONSTR_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "coqcic/lazy_stack.h"

namespace coqcic {

// Abstract base representation class for CIC constrs.
class constr_base;

// Representation classes for different CIC constrs.
class constr_local;
class constr_global;
class constr_builtin;
class constr_product;
class constr_lambda;
class constr_let;
class constr_apply;
class constr_cast;
class constr_match;
class constr_fix;

class type_context_t;

/**
	\brief A coqcic term construction

	Represents a term construct in the Coq calculus of
	inductive constructions.
*/
class constr_t {
public:
	/**
		\brief Construct constr from internally-generated constr type
	*/
	explicit
	inline
	constr_t(
		std::shared_ptr<const constr_base> repr
	) noexcept : repr_(std::move(repr)) {
	}

	constr_t() noexcept = default;

	/**
		\brief Assign constr
	*/
	inline constr_t&
	operator=(constr_t other) noexcept {
		swap(other);
		return *this;
	}

	/**
		\brief Swaps two constr
	*/
	inline void
	swap(constr_t& other) noexcept {
		repr_.swap(other.repr_);
	}

	/**
		\brief Generates human-readable representation

		\param out
			String to append representation to
	*/
	void
	format(std::string& out) const;

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
	operator==(const constr_t& other) const;

	/**
		\brief Compares for inequality with other term
		\param other
			Term to compare to
		\returns
			Inequality comparison result

		Just the opposite of \ref operator==.
	*/
	inline
	bool operator!=(const constr_t& other) const { return ! (*this == other); }

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
	check(const type_context_t& ctx) const;

	/**
		\brief Simplifies term.

		\returns
			Simplified term.

		Resolves apply / lambda pairs to produce a simplified term.
	*/
	constr_t
	simpl() const;

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
	shift(std::size_t limit, int dir) const;

	/**
		\brief Generates a debug string.

		\returns Human-readable string for diagnostic purposes.

		Generates a string that may be useful in understanding the
		structure of a term construction for error diagnostics.
	*/
	std::string
	debug_string() const;

	/**
		\brief Access the underlying representation object
	*/
	const
	std::shared_ptr<const constr_base>&
	repr() const noexcept {
		return repr_;
	}

	std::shared_ptr<const constr_base>
	extract_repr() && noexcept {
		return std::move(repr_);
	}

	/**
		\brief Return \ref constr_local or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_local* as_local() const noexcept;
	/**
		\brief Return \ref constr_global or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_global* as_global() const noexcept;
	/**
		\brief Return \ref constr_builtin or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_builtin* as_builtin() const noexcept;
	/**
		\brief Return \ref constr_product or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_product* as_product() const noexcept;
	/**
		\brief Return \ref constr_lambda or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_lambda* as_lambda() const noexcept;
	/**
		\brief Return \ref constr_let or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_let* as_let() const noexcept;
	/**
		\brief Return \ref constr_apply or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_apply* as_apply() const noexcept;
	/**
		\brief Return \ref constr_cast or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_cast* as_cast() const noexcept;
	/**
		\brief Return \ref constr_match or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_match* as_match() const noexcept;
	/**
		\brief Return \ref constr_fix or nullptr

		Checks the constr for the requested type, returns pointer
		to underlynig structure or nullptr.
	*/
	inline const constr_fix* as_fix() const noexcept;

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

		The visitor may omit handling constructions that it is not interested in.
	*/
	template<typename Visitor>
	inline auto
	visit(Visitor&& vis) const;

private:
	std::shared_ptr<const constr_base> repr_;
};

/**
	\brief A formal argument (to a function)

	Represents a single formal argument of a function (lambda
	or fixpoint).
*/
struct formal_arg_t {
	/**
		\brief Name of the formal argument

		The name assigned in Gallina to this formal argument.
		Note that the name is purely informative, all references
		to arguments are represented as
		\ref de_bruijn_indices "de  Bruijn indices".
	*/
	std::optional<std::string> name;
	constr_t type;

	inline
	bool
	operator==(const formal_arg_t& other) const noexcept {
		return type == other.type;
	}

	inline
	bool
	operator!=(const formal_arg_t& other) const noexcept { return ! (*this == other); }
};

/**
	\brief Single branch of a match expression
*/
struct match_branch_t {
	/**
		\brief id of the constructor to be matched against

		This is the id of the constructor to be matched against.
		It should be the fully qualified name id of the
		constructor, so to resolve the name against the
		list of constructors held in an \ref one_inductive_t
		"inductive definition" this may need to be stripped
		of any module prefix.
	*/
	std::string constructor;

	/**
		\brief Number of arguments matched
	*/
	std::size_t nargs;
	/**
		\brief Match expression

		This is a lambda expression where the first arguments
		are filled in with the extracted constructor arguments.
	*/
	constr_t expr;

	inline bool
	operator==(const match_branch_t& other) const noexcept {
		return
			constructor == other.constructor &&
			nargs == other.nargs &&
			expr == other.expr;
	}
};

/**
	\brief Single function of a mutual fixpoint bundle.
*/
struct fix_function_t {
	/**
		\brief Name of the function within the bundle.

		The name assigned in Gallina to this function.
		Note that the name is purely informative, all references
		to functions are represented as
		\ref de_bruijn_indices "de  Bruijn indices".
	*/
	std::string name;

	/**
		\brief Formal arguments of this function.

		The formal arguments of this function, similar as for
		a \ref constr_lambda "lambda construct". Note however
		that the de Bruijn indices for an entire fix point
		function differs as it must be interpreted relative
		to the context of the fixpoint functions themselves.

		See \ref de_bruijn_fix.
	*/
	std::vector<formal_arg_t> args;
	/**
		\brief Result type of this function.

		The result type of this function -- see \ref de_bruijn_fix
		for interpretation of de Bruijn indices for fixpoint bundles.
	*/
	constr_t restype;
	/**
		\brief Computation body of this function.
		The computation body of this function -- see \ref de_bruijn_fix
		for interpretation of de Bruijn indices for fixpoint bundles.
	*/
	constr_t body;

	inline bool
	operator==(const fix_function_t& other) const noexcept {
		return name == other.name && args == other.args && restype == other.restype && body == other.body;
	}
};

/**
	\brief A bundle of one or more mutually dependent fixpoint functions
*/
struct fix_group_t {
	/**
		\brief The functions in the bundle.
	*/
	std::vector<fix_function_t> functions;

	/**
		\brief Obtain signature of a function in this bundle.

		\param index
			Index of the function, must be within bounds of \ref functions.

		\returns
			Signature of the function.

		Returns the signature of the index'th function of the bundle,
		represented as a dependent product. de Bruijn indices are
		adjusted such that the returned signature can be interpreted
		in the context of the fix expression.

		See \ref de_bruijn_fix for more detailed explanation.
	*/
	constr_t
	get_function_signature(std::size_t index) const;

	inline bool
	operator==(const fix_group_t& other) const noexcept {
		return functions == other.functions;
	}
};

/**
	\brief Collects unresolved de Bruijn indices

	\param obj
		The object to inspect

	\returns
		Collection of all unresolved references (empty if none).

	Collects the de Bruijn indices of all locals in this object that
	are not resolvable within the construct itself. All indices are
	shifted such that they are from the point of view of the
	top-level object.
*/
std::vector<std::size_t>
collect_external_references(const constr_t& obj);

/**
	\brief Context for type checking operations.

	Context for type checking operation on constr objects.
*/
class type_context_t {
public:
	/**
		\brief Create a new type with additional local

		\param name
			Name of the local variable. This is informative only,
			it plays no role in resolution.

		\param type
			Type of the local variable.

		\returns
			New type context object.
	*/
	type_context_t push_local(std::string name, constr_t type) const;

	/**
		\brief Local variable entry.
	*/
	struct local_entry {
		std::string name;
		constr_t type;
	};

	/**
		\brief Stack of local variables.
	*/
	lazy_stack<local_entry> locals;

	/**
		\brief Map constr_global name to its type.
	*/
	std::function<constr_t(const std::string&)> global_types;
};

/**
	\brief Abstract base representation class for CIC constructs.

	Abstract base class for representing the different kinds of CIC
	constructs. Generally, avoid interacting with this directly
	instead of the wrapper \ref constr_t.
*/
class constr_base : public std::enable_shared_from_this<constr_base> {
public:
	virtual
	~constr_base();

	virtual
	void
	format(std::string& out) const = 0;

	virtual
	bool
	operator==(const constr_base& other) const noexcept = 0;

	virtual
	constr_t
	check(const type_context_t& ctx) const = 0;

	virtual
	constr_t
	simpl() const;

	virtual
	constr_t
	shift(std::size_t limit, int dir) const;

	std::string
	repr() const;
};

/**
	\brief Reference to local variable

	Rperesents a reference to a locally bound variable
	(e.g. as a function argument or "let" expression).
*/
class constr_local final : public constr_base {
public:
	~constr_local() override;

	constr_local(
		std::string name,
		std::size_t index);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	inline
	const std::string&
	name() const noexcept { return name_; }

	inline
	std::size_t
	index() const noexcept { return index_; }

private:
	std::string name_;
	std::size_t index_;
};

/**
	\brief Reference to global name
*/
class constr_global final : public constr_base {
public:
	~constr_global() override;

	explicit
	constr_global(
		std::string name);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	inline
	const std::string&
	name() const noexcept { return name_; }

private:
	std::string name_;
};

/**
	\brief Builtin universe type
*/
class constr_builtin final : public constr_base {
public:
	~constr_builtin() override;

	constr_builtin(
		std::string name,
		std::function<constr_t(const constr_base&)> check
	);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	inline
	const std::string&
	name() const noexcept { return name_; }

	static
	std::shared_ptr<const constr_base>
	get_set();

	static
	std::shared_ptr<const constr_base>
	get_prop();

	static
	std::shared_ptr<const constr_base>
	get_sprop();

	static
	std::shared_ptr<const constr_base>
	get_type();

private:
	std::string name_;
	std::function<constr_t(const constr_base&)> check_;
};

/**
	\brief Dependent product.
*/
class constr_product final : public constr_base {
public:
	~constr_product() override;

	constr_product(std::vector<formal_arg_t> args, constr_t restype);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	/**
		\brief Formal argument of the product

		See \ref de_bruijn_product for details on interpreting
		the arguments.
	*/
	inline
	const std::vector<formal_arg_t>&
	args() const noexcept { return args_; }

	/**
		\brief Result type of the product

		See \ref de_bruijn_product for details on interpreting
		the arguments.
	*/
	inline
	const constr_t&
	restype() const noexcept { return restype_; }

private:
	std::vector<formal_arg_t> args_;
	constr_t restype_;
};

/**
	\brief Lambda abstraction.
*/
class constr_lambda final : public constr_base {
public:
	~constr_lambda() override;

	constr_lambda(std::vector<formal_arg_t> args, constr_t body);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	inline
	const std::vector<formal_arg_t>&
	args() const noexcept { return args_; }

	inline
	const constr_t&
	body() const noexcept { return body_; }

private:
	std::vector<formal_arg_t> args_;
	constr_t argtype_;
	constr_t body_;
};

/**
	\brief Let binding.
*/
class constr_let final : public constr_base {
public:
	~constr_let() override;

	constr_let(
		std::optional<std::string> varname,
		constr_t value,
		constr_t type,
		constr_t body
	);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	inline
	const std::optional<std::string>&
	varname() const noexcept { return varname_; }

	inline
	const constr_t&
	value() const noexcept { return value_; }

	inline
	const constr_t&
	type() const noexcept { return type_; }

	inline
	const constr_t&
	body() const noexcept { return body_; }

private:
	std::optional<std::string> varname_;
	constr_t value_;
	constr_t type_;
	constr_t body_;
};

/**
	\brief Functional application (call).
*/
class constr_apply final : public constr_base {
public:
	~constr_apply() override;

	constr_apply(constr_t fn, std::vector<constr_t> args);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	simpl() const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	inline
	const constr_t&
	fn() const noexcept { return fn_; }

	inline
	const std::vector<constr_t>&
	args() const noexcept { return args_; }

private:
	constr_t fn_;
	std::vector<constr_t> args_;
};

/**
	\brief Cast expression.
*/
class constr_cast final : public constr_base {
public:
	enum kind_type {
		vm_cast,
		default_cast,
		revert_cast,
		native_cast
	};

	~constr_cast() override;

	constr_cast(constr_t term, kind_type kind, constr_t typeterm);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	inline
	const constr_t&
	term() const noexcept { return term_; }

	inline
	kind_type
	kind() const noexcept { return kind_; }

	inline
	const constr_t&
	typeterm() const noexcept { return typeterm_; }

private:
	constr_t term_;
	kind_type kind_;
	constr_t typeterm_;
};

/**
	\brief Pattern matching expression.
*/
class constr_match final : public constr_base {
public:
	~constr_match() override;

	constr_match(constr_t casetype, constr_t arg, std::vector<match_branch_t> branches);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	// The type of the case expression abstracted over its argument -- i.e.
	// this is a lambda type taking the match expression as the first argument
	// and producing the result type of the match expression.
	inline
	const constr_t&
	casetype() const noexcept { return casetype_; }

	// The actual argument of the case expression.
	inline
	const constr_t&
	arg() const noexcept { return arg_; }

	// The branches of the expression. Each branch must be a lambda expression
	// that is applied to the constructor arguments of the corresponding case.
	// (Type) arguments to the inductive type itself are not included as
	// arguments.
	inline
	const std::vector<match_branch_t>&
	branches() const noexcept { return branches_; }

private:
	constr_t casetype_;
	constr_t arg_;
	std::vector<match_branch_t> branches_;
};

/**
	\brief Mutual fixpoint function group.
*/
class constr_fix final : public constr_base {
public:
	~constr_fix() override;

	constr_fix(std::size_t index, std::shared_ptr<const fix_group_t> group);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr_t
	check(const type_context_t& ctx) const override;

	constr_t
	shift(std::size_t limit, int dir) const override;

	inline
	std::size_t
	index() const noexcept { return index_; }

	inline
	const std::shared_ptr<const fix_group_t>&
	group() const noexcept { return group_; }

private:
	std::size_t index_;
	std::shared_ptr<const fix_group_t> group_;
};

////////////////////////////////////////////////////////////////////////////////
// constr_t implementations

const constr_local* constr_t::as_local() const noexcept { return dynamic_cast<const constr_local*>(repr_.get()); }
const constr_global* constr_t::as_global() const noexcept { return dynamic_cast<const constr_global*>(repr_.get()); }
const constr_builtin* constr_t::as_builtin() const noexcept { return dynamic_cast<const constr_builtin*>(repr_.get()); }
const constr_product* constr_t::as_product() const noexcept { return dynamic_cast<const constr_product*>(repr_.get()); }
const constr_lambda* constr_t::as_lambda() const noexcept { return dynamic_cast<const constr_lambda*>(repr_.get()); }
const constr_let* constr_t::as_let() const noexcept { return dynamic_cast<const constr_let*>(repr_.get()); }
const constr_apply* constr_t::as_apply() const noexcept { return dynamic_cast<const constr_apply*>(repr_.get()); }
const constr_cast* constr_t::as_cast() const noexcept { return dynamic_cast<const constr_cast*>(repr_.get()); }
const constr_match* constr_t::as_match() const noexcept { return dynamic_cast<const constr_match*>(repr_.get()); }
const constr_fix* constr_t::as_fix() const noexcept { return dynamic_cast<const constr_fix*>(repr_.get()); }

template<typename Visitor>
inline auto
constr_t::visit(Visitor&& vis) const {
	if (auto local = as_local()) {
		return vis(*local);
	} else if (auto global = as_global()) {
		return vis(*global);
	} else if (auto builtin = as_builtin()) {
		return vis(*builtin);
	} else if (auto product = as_product()) {
		return vis(*product);
	} else if (auto lambda = as_lambda()) {
		return vis(*lambda);
	} else if (auto let = as_let()) {
		return vis(*let);
	} else if (auto apply = as_apply()) {
		return vis(*apply);
	} else if (auto cast = as_cast()) {
		return vis(*cast);
	} else if (auto match_case = as_match()) {
		return vis(*match_case);
	} else if (auto fix = as_fix()) {
		return vis(*fix);
	} else {
		std::terminate();
	}
}

////////////////////////////////////////////////////////////////////////////////
// builder helpers for constructs

namespace builder {

constr_t
local(std::string name, std::size_t index);

constr_t
global(std::string name);

constr_t
builtin_set();

constr_t
builtin_prop();

constr_t
builtin_sprop();

constr_t
builtin_type();

constr_t
let(std::optional<std::string> varname, constr_t value, constr_t body);

constr_t
product(std::vector<formal_arg_t> args, constr_t restype);

constr_t
lambda(std::vector<formal_arg_t> args, constr_t body);

constr_t
let(std::optional<std::string> varname, constr_t value, constr_t type, constr_t body);

constr_t
apply(constr_t fn, std::vector<constr_t> arg);

constr_t
cast(constr_t term, constr_cast::kind_type kind, constr_t typeterm);

constr_t
match(constr_t restype, constr_t arg, std::vector<match_branch_t> branches);

constr_t
fix(std::size_t index, std::shared_ptr<const fix_group_t> group);

}  // builder

}  // namespace coqcic

#endif  // COQCIC_CONSTR_H
