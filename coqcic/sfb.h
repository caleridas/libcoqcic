#ifndef COQCIC_SFB_H
#define COQCIC_SFB_H

#include <memory>
#include <string>
#include <vector>

#include "coqcic/constr.h"

namespace coqcic {

class sfb_repr;

class inductive_repr;
class definition_repr;
class axiom_repr;
class module_repr;
class module_type_repr;

class sfb {
public:
	explicit
	inline
	sfb(std::shared_ptr<const sfb_repr> repr) noexcept
		: repr_(std::move(repr))
	{
	}

	sfb() noexcept = default;

	inline sfb&
	operator=(sfb other) noexcept
	{
		swap(other);
		return *this;
	}

	inline void
	swap(sfb& other) noexcept
	{
		repr_.swap(other.repr_);
	}

	void
	format(std::string& out) const;

	bool
	operator==(const sfb& other) const noexcept;

	inline
	bool operator!=(const sfb& other) const
	{
		return ! (*this == other);
	}

	std::string
	debug_string() const;

	const
	std::shared_ptr<const sfb_repr>&
	repr() const noexcept
	{
		return repr_;
	}

	std::shared_ptr<const sfb_repr>
	extract_repr() && noexcept
	{
		return std::move(repr_);
	}

	inline const definition_repr* as_definition() const noexcept;
	inline const axiom_repr* as_axiom() const noexcept;
	inline const inductive_repr* as_inductive() const noexcept;
	inline const module_repr* as_module() const noexcept;
	inline const module_type_repr* as_module_type() const noexcept;

	template<typename Visitor>
	inline auto
	visit(Visitor&& vis);

private:
	std::shared_ptr<const sfb_repr> repr_;
};

class sfb_repr : public std::enable_shared_from_this<sfb_repr> {
public:
	virtual
	~sfb_repr();

	virtual
	void
	format(std::string& out) const = 0;

	virtual
	bool
	operator==(const sfb_repr& other) const noexcept = 0;

	std::string
	repr() const;
};

class definition_repr final : public sfb_repr {
public:
	~definition_repr() override;

	inline
	definition_repr(std::string id, constr type, constr value) noexcept
		: id_(std::move(id)), type_(std::move(type)), value_(std::move(value))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const sfb_repr& other) const noexcept override;

	inline const std::string&
	id() const noexcept
	{
		return id_;
	}

	inline const constr&
	type() const noexcept
	{
		return type_;
	}

	inline const constr&
	value() const noexcept
	{
		return value_;
	}

private:
	std::string id_;
	constr type_;
	constr value_;
};

class axiom_repr final : public sfb_repr {
public:
	~axiom_repr() override;

	inline
	axiom_repr(std::string id, constr type) noexcept
		: id_(std::move(id)), type_(std::move(type))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const sfb_repr& other) const noexcept override;
	inline const std::string&

	id() const noexcept
	{
		return id_;
	}

	inline const constr&
	type() const noexcept
	{
		return type_;
	}

private:
	std::string id_;
	constr type_;
};

struct one_inductive {
	struct constructor {
		std::string id;
		constr type;

		inline bool
		operator==(const constructor& other) const noexcept
		{
			return id == other.id && type == other.type;
		}

		inline bool
		operator!=(const constructor& other) const noexcept
		{
			return !(*this == other);
		}
	};

	std::string id;
	constr type;
	std::vector<constructor> constructors;

	inline
	one_inductive(
		std::string init_id,
		constr init_type,
		std::vector<constructor> init_constructors) noexcept
		: id(std::move(init_id)), type(std::move(init_type)), constructors(std::move(init_constructors))
	{
	}

	inline bool
	operator==(const one_inductive& other) const noexcept
	{
		return id == other.id && type == other.type && constructors == other.constructors;
	}

	inline bool
	operator!=(const one_inductive& other) const noexcept
	{
		return !(*this == other);
	}
};

class inductive_repr final : public sfb_repr {
public:
	~inductive_repr() override;

	inline
	inductive_repr(std::vector<one_inductive> one_inductives) noexcept
		: one_inductives_(std::move(one_inductives))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const sfb_repr& other) const noexcept override;

	inline const std::vector<one_inductive>&
	one_inductives() const noexcept
	{
		return one_inductives_;
	}

private:
	std::vector<one_inductive> one_inductives_;
};

// Algorithmic module expression: A module name,  possibly with arguments
// that it is applied to (if it is a parameterized module).
struct modexpr {
	std::string name;
	std::vector<std::string> args;

	inline bool
	operator==(const modexpr& other) const noexcept
	{
		return name == other.name && args == other.args;
	}

	inline bool
	operator!=(const modexpr& other) const noexcept
	{
		return !(*this == other);
	}

	void
	format(std::string& out) const;
};

class module_body_repr;
class module_body_algebraic_repr;
class module_body_struct_repr;

class module_body {
public:
	inline
	module_body(
		std::vector<std::pair<std::string, modexpr>> parameters,
		std::shared_ptr<const module_body_repr> repr) noexcept
		: parameters_(std::move(parameters)), repr_(std::move(repr))
	{
	}

	module_body() noexcept = default;
	inline module_body(const module_body& other) = default;
	inline module_body(module_body&& other) noexcept = default;
	inline module_body& operator=(const module_body& other) = default;
	inline module_body& operator=(module_body&& other) noexcept = default;

	inline void
	swap(module_body& other) noexcept
	{
		parameters_.swap(other.parameters_);
		repr_.swap(other.repr_);
	}

	void
	format(std::string& out) const;

	bool
	operator==(const module_body& other) const noexcept;

	inline
	bool operator!=(const module_body& other) const
	{
		return ! (*this == other);
	}

	std::string
	debug_string() const;

	inline
	const std::vector<std::pair<std::string, modexpr>>&
	parameters() const noexcept
	{
		return parameters_;
	}

	inline const
	std::shared_ptr<const module_body_repr>&
	repr() const noexcept
	{
		return repr_;
	}

	inline std::shared_ptr<const module_body_repr>
	extract_repr() && noexcept
	{
		return std::move(repr_);
	}

private:
	std::vector<std::pair<std::string, modexpr>> parameters_;
	std::shared_ptr<const module_body_repr> repr_;
};

class module_body_repr : std::enable_shared_from_this<module_body_repr> {
public:
	virtual
	~module_body_repr();

	virtual
	void
	format(std::string& out) const = 0;

	virtual
	bool
	operator==(const module_body_repr& other) const noexcept = 0;

	std::string
	repr() const;
};

class module_body_algebraic_repr final : public module_body_repr {
public:
	~module_body_algebraic_repr() override;

	explicit inline
	module_body_algebraic_repr(
		modexpr expr) noexcept
		: expr_(std::move(expr))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const module_body_repr& other) const noexcept override;

	inline const std::string& id() const noexcept { return id_; }
	inline const modexpr& expr() const noexcept { return expr_; }

private:
	std::string id_;
	modexpr expr_;
};

class module_body_struct_repr final : public module_body_repr {
public:
	~module_body_struct_repr() override;

	inline
	module_body_struct_repr(
		std::optional<modexpr> type,
		std::vector<sfb> body) noexcept
		: type_(std::move(type)), body_(std::move(body))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const module_body_repr& other) const noexcept override;

	inline const std::optional<modexpr>& type() const noexcept { return type_; }
	inline const std::vector<sfb>& body() const noexcept { return body_; }

private:
	std::optional<modexpr> type_;
	std::vector<sfb> body_;
};

class module_repr final : public sfb_repr {
public:
	~module_repr() override;

	inline
	module_repr(std::string id, module_body body) noexcept
		: id_(std::move(id)), body_(std::move(body))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const sfb_repr& other) const noexcept override;

	inline const std::string& id() const noexcept { return id_; }
	inline const module_body& body() const noexcept { return body_; }

private:
	std::string id_;
	module_body body_;
};

class module_type_repr final : public sfb_repr {
public:
	~module_type_repr() override;

	inline
	module_type_repr(std::string id, module_body body) noexcept
		: id_(std::move(id)), body_(std::move(body))
	{
	}

	void
	format(std::string& out) const override;

	bool
	operator==(const sfb_repr& other) const noexcept override;

	inline const std::string& id() const noexcept { return id_; }
	inline const module_body& body() const noexcept { return body_; }

private:
	std::string id_;
	// XXX: not quite correct -- can never have a type, must always be structural.
	module_body body_;
};

inline const definition_repr*
sfb::as_definition() const noexcept
{
	return dynamic_cast<const definition_repr*>(repr_.get());
}

inline const axiom_repr*
sfb::as_axiom() const noexcept
{
	return dynamic_cast<const axiom_repr*>(repr_.get());
}

inline const inductive_repr*
sfb::as_inductive() const noexcept
{
	return dynamic_cast<const inductive_repr*>(repr_.get());
}

inline const module_repr*
sfb::as_module() const noexcept
{
	return dynamic_cast<const module_repr*>(repr_.get());
}

inline const module_type_repr*
sfb::as_module_type() const noexcept
{
	return dynamic_cast<const module_type_repr*>(repr_.get());
}

template<typename Visitor>
inline auto
sfb::visit(Visitor&& vis)
{
	if (auto def = as_definition()) {
		return vis(*def);
	} else if (auto axiom = as_axiom()) {
		return vis(*axiom);
	} else if (auto inductive = as_inductive()) {
		return vis(*inductive);
	} else if (auto module_def = as_module()) {
		return vis(*module_def);
	} else if (auto module_type_def = as_module_type()) {
		return vis(*module_type_def);
	} else {
		std::terminate();
	}
}

namespace builder {

sfb
definition(std::string id, constr type, constr value);

sfb
axiom(std::string id, constr type);

sfb
inductive(std::vector<one_inductive> one_inductives);

sfb
module_def(std::string id, module_body body);

sfb
module_type_def(std::string id, module_body body);

}  // builder

}  // namespace coqcic

#endif  // COQCIC_SFB_H
