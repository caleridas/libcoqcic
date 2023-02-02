#ifndef COQCIC_CONSTR_H
#define COQCIC_CONSTR_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "coqcic/shared_stack.h"

namespace coqcic {

class constr_repr;

class local_repr;
class global_repr;
class builtin_repr;
class product_repr;
class lambda_repr;
class let_repr;
class apply_repr;
class cast_repr;
class case_repr;
class fix_repr;

class type_context;

class constr {
public:
	explicit
	inline
	constr(std::shared_ptr<const constr_repr> repr) noexcept
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
	std::shared_ptr<const constr_repr>&
	repr() const noexcept
	{
		return repr_;
	}

	std::shared_ptr<const constr_repr>
	extract_repr() && noexcept
	{
		return std::move(repr_);
	}

	inline const local_repr* as_local() const noexcept;
	inline const global_repr* as_global() const noexcept;
	inline const builtin_repr* as_builtin() const noexcept;
	inline const product_repr* as_product() const noexcept;
	inline const lambda_repr* as_lambda() const noexcept;
	inline const let_repr* as_let() const noexcept;
	inline const apply_repr* as_apply() const noexcept;
	inline const cast_repr* as_cast() const noexcept;
	inline const case_repr* as_case() const noexcept;
	inline const fix_repr* as_fix() const noexcept;

	template<typename Visitor>
	inline auto
	visit(Visitor&& vis);

private:
	std::shared_ptr<const constr_repr> repr_;
};

struct formal_arg {
	std::optional<std::string> name;
	constr type;

	inline
	bool
	operator==(const formal_arg& other) const noexcept
	{
		return type == other.type;
	}

	inline
	bool
	operator!=(const formal_arg& other) const noexcept { return ! (*this == other); }
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

	// Map global_repr name to its type
	std::function<constr(const std::string&)> global_types;
};

class constr_repr : public std::enable_shared_from_this<constr_repr> {
public:
	virtual
	~constr_repr();

	virtual
	void
	format(std::string& out) const = 0;

	virtual
	bool
	operator==(const constr_repr& other) const noexcept = 0;

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

class local_repr final : public constr_repr {
public:
	~local_repr() override;

	local_repr(
		std::string name,
		std::size_t index);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

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

class global_repr final : public constr_repr {
public:
	~global_repr() override;

	explicit
	global_repr(
		std::string name);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	inline
	const std::string&
	name() const noexcept { return name_; }

private:
	std::string name_;
};

class builtin_repr final : public constr_repr {
public:
	~builtin_repr() override;

	builtin_repr(
		std::string name,
		std::function<constr(const constr_repr&)> check);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	inline
	const std::string&
	name() const noexcept { return name_; }

	static
	std::shared_ptr<const constr_repr>
	get_set();

	static
	std::shared_ptr<const constr_repr>
	get_prop();

	static
	std::shared_ptr<const constr_repr>
	get_sprop();

	static
	std::shared_ptr<const constr_repr>
	get_type();

private:
	std::string name_;
	std::function<constr(const constr_repr&)> check_;
};

class product_repr final : public constr_repr {
public:
	~product_repr() override;

	product_repr(std::vector<formal_arg> args, constr restype);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const std::vector<formal_arg>&
	args() const noexcept { return args_; }

	inline
	const constr&
	restype() const noexcept { return restype_; }

private:
	std::vector<formal_arg> args_;
	constr restype_;
};

class lambda_repr final : public constr_repr {
public:
	~lambda_repr() override;

	lambda_repr(std::vector<formal_arg> args, constr body);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	const std::vector<formal_arg>&
	args() const noexcept { return args_; }

	inline
	const constr&
	body() const noexcept { return body_; }

private:
	std::vector<formal_arg> args_;
	constr argtype_;
	constr body_;
};

class let_repr final : public constr_repr {
public:
	~let_repr() override;

	let_repr(
		std::optional<std::string> varname,
		constr value,
		constr body);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

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

class apply_repr final : public constr_repr {
public:
	~apply_repr() override;

	apply_repr(constr fn, std::vector<constr> args);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

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

class cast_repr final : public constr_repr {
public:
	enum kind_type {
		vm_cast,
		default_cast,
		revert_cast,
		native_cast
	};

	~cast_repr() override;

	cast_repr(constr term, kind_type kind, constr typeterm);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

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

class case_repr final : public constr_repr {
public:
	struct branch {
		std::string constructor;
		std::size_t nargs;
		constr expr;

		inline bool
		operator==(const branch& other) const noexcept
		{
			return
				constructor == other.constructor &&
				nargs == other.nargs &&
				expr == other.expr;
		}
	};

	~case_repr() override;

	case_repr(constr restype, constr arg, std::vector<branch> branches);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

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
	const std::vector<branch>&
	branches() const noexcept { return branches_; }

private:
	constr restype_;
	constr arg_;
	std::vector<branch> branches_;
};

struct fix_group {
	struct function {
		std::string name;
		std::vector<formal_arg> args;
		constr restype;
		constr body;

		inline bool
		operator==(const function& other) const noexcept
		{
			return name == other.name && args == other.args && restype == other.restype && body == other.body;
		}
	};

	std::vector<function> functions;

	inline bool
	operator==(const fix_group& other) const noexcept
	{
		return functions == other.functions;
	}
};

class fix_repr final : public constr_repr {
public:
	~fix_repr() override;

	fix_repr(std::size_t index, std::shared_ptr<const fix_group> group);

	void
	format(std::string& out) const override;

	bool
	operator==(const constr_repr& other) const noexcept override;

	constr
	check(const type_context& ctx) const override;

	constr
	shift(std::size_t limit, int dir) const override;

	inline
	std::size_t
	index() const noexcept { return index_; }

	inline
	const std::shared_ptr<const fix_group>&
	group() const noexcept { return group_; }

private:
	std::size_t index_;
	std::shared_ptr<const fix_group> group_;
};

////////////////////////////////////////////////////////////////////////////////
// constr implementations

const local_repr* constr::as_local() const noexcept { return dynamic_cast<const local_repr*>(repr_.get()); }
const global_repr* constr::as_global() const noexcept { return dynamic_cast<const global_repr*>(repr_.get()); }
const builtin_repr* constr::as_builtin() const noexcept { return dynamic_cast<const builtin_repr*>(repr_.get()); }
const product_repr* constr::as_product() const noexcept { return dynamic_cast<const product_repr*>(repr_.get()); }
const lambda_repr* constr::as_lambda() const noexcept { return dynamic_cast<const lambda_repr*>(repr_.get()); }
const let_repr* constr::as_let() const noexcept { return dynamic_cast<const let_repr*>(repr_.get()); }
const apply_repr* constr::as_apply() const noexcept { return dynamic_cast<const apply_repr*>(repr_.get()); }
const cast_repr* constr::as_cast() const noexcept { return dynamic_cast<const cast_repr*>(repr_.get()); }
const case_repr* constr::as_case() const noexcept { return dynamic_cast<const case_repr*>(repr_.get()); }
const fix_repr* constr::as_fix() const noexcept { return dynamic_cast<const fix_repr*>(repr_.get()); }

template<typename Visitor>
inline auto
constr::visit(Visitor&& vis)
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
	} else if (auto match_case = as_case()) {
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
product(std::vector<formal_arg> args, constr restype);

constr
lambda(std::vector<formal_arg> args, constr body);

constr
let(std::optional<std::string> varname, constr value, constr body);

constr
apply(constr fn, std::vector<constr> arg);

constr
cast(constr term, cast_repr::kind_type kind, constr typeterm);

constr
case_match(constr restype, constr arg, std::vector<case_repr::branch> branches);

constr
fix(std::size_t index, std::shared_ptr<const fix_group> group);

}  // builder

}  // namespace coqcic

#endif  // COQCIC_CONSTR_H
