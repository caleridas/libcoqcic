#ifndef COQCIC_CONSTR_H
#define COQCIC_CONSTR_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "coqcic/shared_stack.h"

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

class type_context;

// A CIC constr, represented as one of the representation
// classes above.
class constr {
public:
	explicit
	inline
	constr(std::shared_ptr<const constr_base> repr) noexcept
		: repr_(std::move(repr))
	{
	}

	constr() noexcept = default;

	inline constr&
	operator=(constr other) noexcept
	{
		swap(other);
		return *this;
	}

	inline void
	swap(constr& other) noexcept
	{
		repr_.swap(other.repr_);
	}

	void
	format(std::string& out) const;

	bool
	operator==(const constr& other) const;

	inline
	bool operator!=(const constr& other) const { return ! (*this == other); }

	constr
	check(const type_context& ctx) const;

	constr
	simpl() const;

	constr
	shift(std::size_t limit, int dir) const;

	std::string
	debug_string() const;

	const
	std::shared_ptr<const constr_base>&
	repr() const noexcept
	{
		return repr_;
	}

	std::shared_ptr<const constr_base>
	extract_repr() && noexcept
	{
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
	constr type;

	inline
	bool
	operator==(const formal_arg_t& other) const noexcept
	{
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
	constr expr;

	inline bool
	operator==(const match_branch_t& other) const noexcept
	{
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
	constr restype;
	// Body of this function (dependent on all functions in this bundle as well as the args).
	constr body;

	inline bool
	operator==(const fix_function_t& other) const noexcept
	{
		return name == other.name && args == other.args && restype == other.restype && body == other.body;
	}
};

// A bundle fixpoint functions that mutually depend on each other.
struct fix_group_t {
	using function = fix_function_t;
	std::vector<fix_function_t> functions;

	inline bool
	operator==(const fix_group_t& other) const noexcept
	{
		return functions == other.functions;
	}
};

// Collect the de Bruijn indices of all locals in this object that
// are not resolvable within the construct itself.
std::vector<std::size_t>
collect_external_references(const constr& obj);

class type_context {
public:
	type_context push_local(std::string name, constr type) const;

	struct local_entry {
		std::string name;
		constr type;
	};

	shared_stack<local_entry> locals;

	// Map constr_global name to its type
	std::function<constr(const std::string&)> global_types;
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
	constr
	check(const type_context& ctx) const = 0;

	virtual
	constr
	simpl() const;

	virtual
	constr
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

	constr
	check(const type_context& ctx) const override;

	constr
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

	constr
	check(const type_context& ctx) const override;

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
		std::function<constr(const constr_base&)> check);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

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
	std::function<constr(const constr_base&)> check_;
};

class constr_product final : public constr_base {
public:
	~constr_product() override;

	constr_product(std::vector<formal_arg_t> args, constr restype);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const std::vector<formal_arg_t>&
	args() const noexcept { return args_; }

	inline
	const constr&
	restype() const noexcept { return restype_; }

private:
	std::vector<formal_arg_t> args_;
	constr restype_;
};

class constr_lambda final : public constr_base {
public:
	~constr_lambda() override;

	constr_lambda(std::vector<formal_arg_t> args, constr body);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const std::vector<formal_arg_t>&
	args() const noexcept { return args_; }

	inline
	const constr&
	body() const noexcept { return body_; }

private:
	std::vector<formal_arg_t> args_;
	constr argtype_;
	constr body_;
};

class constr_let final : public constr_base {
public:
	~constr_let() override;

	constr_let(
		std::optional<std::string> varname,
		constr value,
		constr body);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const std::optional<std::string>&
	varname() const noexcept { return varname_; }

	inline
	const constr&
	value() const noexcept { return value_; }

	inline
	const constr&
	body() const noexcept { return body_; }

private:
	std::optional<std::string> varname_;
	constr value_;
	constr body_;
};

class constr_apply final : public constr_base {
public:
	~constr_apply() override;

	constr_apply(constr fn, std::vector<constr> args);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	simpl() const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const constr&
	fn() const noexcept { return fn_; }

	inline
	const std::vector<constr>&
	args() const noexcept { return args_; }

private:
	constr fn_;
	std::vector<constr> args_;
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

	constr_cast(constr term, kind_type kind, constr typeterm);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const constr&
	term() const noexcept { return term_; }

	inline
	kind_type
	kind() const noexcept { return kind_; }

	inline
	const constr&
	typeterm() const noexcept { return typeterm_; }

private:
	constr term_;
	kind_type kind_;
	constr typeterm_;
};

class constr_match final : public constr_base {
public:
	~constr_match() override;

	constr_match(constr restype, constr arg, std::vector<match_branch_t> branches);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_base& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	// The result type of the case expression. Note that this is an expression
	// dependent on the argument type.
	inline
	const constr&
	restype() const noexcept { return restype_; }

	// The actual argument of the case expression.
	inline
	const constr&
	arg() const noexcept { return arg_; }

	// The branches of the expression. Each branch must be a lambda expression
	// that is applied to the constructor arguments of the corresponding case.
	// (Type) arguments to the inductive type itself are not included as
	// arguments.
	inline
	const std::vector<match_branch_t>&
	branches() const noexcept { return branches_; }

private:
	constr restype_;
	constr arg_;
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

	constr
	check(const type_context& ctx) const override;

	constr
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
// constr implementations

const constr_local* constr::as_local() const noexcept { return dynamic_cast<const constr_local*>(repr_.get()); }
const constr_global* constr::as_global() const noexcept { return dynamic_cast<const constr_global*>(repr_.get()); }
const constr_builtin* constr::as_builtin() const noexcept { return dynamic_cast<const constr_builtin*>(repr_.get()); }
const constr_product* constr::as_product() const noexcept { return dynamic_cast<const constr_product*>(repr_.get()); }
const constr_lambda* constr::as_lambda() const noexcept { return dynamic_cast<const constr_lambda*>(repr_.get()); }
const constr_let* constr::as_let() const noexcept { return dynamic_cast<const constr_let*>(repr_.get()); }
const constr_apply* constr::as_apply() const noexcept { return dynamic_cast<const constr_apply*>(repr_.get()); }
const constr_cast* constr::as_cast() const noexcept { return dynamic_cast<const constr_cast*>(repr_.get()); }
const constr_match* constr::as_match() const noexcept { return dynamic_cast<const constr_match*>(repr_.get()); }
const constr_fix* constr::as_fix() const noexcept { return dynamic_cast<const constr_fix*>(repr_.get()); }

template<typename Visitor>
inline auto
constr::visit(Visitor&& vis) const
{
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
// inductive types

class inductive;

class constructor {
public:
	inline constructor(std::string name, constr type) noexcept
		: name_(std::move(name)), index_(0), type_(std::move(type))
	{
	}

	const std::string& name() const noexcept { return name_; }
	const constr& type() const noexcept { return type_; }
	const std::size_t index() const noexcept { return index_; }

private:
	std::string name_;
	std::size_t index_;
	constr type_;

	friend class inductive;
};

class inductive {
public:
	inline inductive(std::string name, constr sig, std::size_t bound, std::vector<constructor> constructors) noexcept
		: name_(std::move(name)), sig_(std::move(sig)), bound_(bound), constructors_(std::move(constructors))
	{
		for (std::size_t n = 0; n < constructors_.size(); ++n) {
			constructors_[n].index_ = n;
		}
	}

	const std::string& name() const noexcept { return name_; }
	inline const constr& sig() const noexcept { return sig_; }
	inline const std::size_t bound() const noexcept { return bound_; }
	inline const std::vector<constructor>& constructors() const noexcept { return constructors_; }

private:
	std::string name_;
	constr sig_;
	std::size_t bound_;
	std::vector<constructor> constructors_;
};

class mutual_inductive {
public:
	explicit inline
	mutual_inductive(std::vector<inductive> inductives) noexcept
		: inductives_(inductives)
	{
	}

	const std::vector<inductive>& inductives() const noexcept { return inductives_; }

private:
	std::vector<inductive> inductives_;
};

////////////////////////////////////////////////////////////////////////////////
// global references

struct globref_inductive {
	std::shared_ptr<const mutual_inductive> mind;
	std::size_t inductive_index;
};

struct globref_constructor {
	std::shared_ptr<const mutual_inductive> mind;
	std::size_t inductive_index;
	std::size_t constructor_index;
};

struct globref_expr {
	constr expr;
};

using globref = std::variant<globref_expr, globref_inductive, globref_constructor>;

using globals_registry = std::function<globref(const std::string&)>;

////////////////////////////////////////////////////////////////////////////////
// builder helpers for constructs

namespace builder {

constr
local(std::string name, std::size_t index);

constr
global(std::string name);

constr
builtin_set();

constr
builtin_prop();

constr
builtin_sprop();

constr
builtin_type();

constr
let(std::optional<std::string> varname, constr value, constr body);

constr
product(std::vector<formal_arg_t> args, constr restype);

constr
lambda(std::vector<formal_arg_t> args, constr body);

constr
let(std::optional<std::string> varname, constr value, constr body);

constr
apply(constr fn, std::vector<constr> arg);

constr
cast(constr term, constr_cast::kind_type kind, constr typeterm);

constr
match(constr restype, constr arg, std::vector<match_branch_t> branches);

constr
fix(std::size_t index, std::shared_ptr<const fix_group_t> group);

}  // builder

}  // namespace coqcic

#endif  // COQCIC_CONSTR_H
