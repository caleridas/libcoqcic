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

class constr_t {
public:
	explicit
	inline
	constr_t(
		std::shared_ptr<const constr_base> repr
	) noexcept : repr_(std::move(repr)) {
	}

	constr_t() noexcept = default;

	inline constr_t&
	operator=(constr_t other) noexcept {
		swap(other);
		return *this;
	}

	inline void
	swap(constr_t& other) noexcept {
		repr_.swap(other.repr_);
	}

	void
	format(std::string& out) const;

	bool
	operator==(const constr_t& other) const;

	inline
	bool operator!=(const constr_t& other) const { return ! (*this == other); }

	constr_t
	check(const type_context_t& ctx) const;

	constr_t
	simpl() const;

	constr_t
	shift(std::size_t limit, int dir) const;

	std::string
	debug_string() const;

	const
	std::shared_ptr<const constr_base>&
	repr() const noexcept {
		return repr_;
	}

	std::shared_ptr<const constr_base>
	extract_repr() && noexcept {
		return std::move(repr_);
	}

	inline const constr_local* as_local() const noexcept;
	inline const constr_global* as_global() const noexcept;
	inline const constr_builtin* as_builtin() const noexcept;
	inline const constr_product* as_product() const noexcept;
	inline const constr_lambda* as_lambda() const noexcept;
	inline const constr_let* as_let() const noexcept;
	inline const constr_apply* as_apply() const noexcept;
	inline const constr_cast* as_cast() const noexcept;
	inline const constr_match* as_match() const noexcept;
	inline const constr_fix* as_fix() const noexcept;

	template<typename Visitor>
	inline auto
	visit(Visitor&& vis) const;

private:
	std::shared_ptr<const constr_base> repr_;
};

// A formal argument to a function.
struct formal_arg_t {
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

// A single case branch of a "match" expression.
struct match_branch_t {
	// Id of the constructor that is matched on.
	std::string constructor;
	// Number of arguments matched.
	std::size_t nargs;
	// Lambda expression of the match.
	constr_t expr;

	inline bool
	operator==(const match_branch_t& other) const noexcept {
		return
			constructor == other.constructor &&
			nargs == other.nargs &&
			expr == other.expr;
	}
};

// A single function of a mutual fixpoint bundle.
struct fix_function_t {
	// Name of the function within the bundle.
	std::string name;
	// Formal arguments of this function.
	std::vector<formal_arg_t> args;
	// Result type of this function (dependent on args).
	constr_t restype;
	// Body of this function (dependent on all functions in this bundle as well as the args).
	constr_t body;

	inline bool
	operator==(const fix_function_t& other) const noexcept {
		return name == other.name && args == other.args && restype == other.restype && body == other.body;
	}
};

// A bundle fixpoint functions that mutually depend on each other.
struct fix_group_t {
	using function = fix_function_t;
	std::vector<fix_function_t> functions;

	inline bool
	operator==(const fix_group_t& other) const noexcept {
		return functions == other.functions;
	}
};

// Collect the de Bruijn indices of all locals in this object that
// are not resolvable within the construct itself.
std::vector<std::size_t>
collect_external_references(const constr_t& obj);

// Context for type checking operation on constr objects.
class type_context_t {
public:
	type_context_t push_local(std::string name, constr_t type) const;

	struct local_entry {
		std::string name;
		constr_t type;
	};

	lazy_stack<local_entry> locals;

	// Map constr_global name to its type
	std::function<constr_t(const std::string&)> global_types;
};

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

	inline
	const std::vector<formal_arg_t>&
	args() const noexcept { return args_; }

	inline
	const constr_t&
	restype() const noexcept { return restype_; }

private:
	std::vector<formal_arg_t> args_;
	constr_t restype_;
};

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
	// this is a product type taking the match expression as the first argument
	// and producing the result type of the match expression (applied to the
	// match argument).
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
