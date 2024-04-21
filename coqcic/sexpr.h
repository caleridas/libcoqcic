#ifndef COQCIC_SEXPR_H
#define COQCIC_SEXPR_H

#include <memory>
#include <string>
#include <vector>

namespace coqcic {

class sexpr_compound;
class sexpr_terminal;
class sexpr_repr;

class sexpr {
public:
	sexpr() = default;

	inline
	sexpr(const sexpr& other);

	sexpr(sexpr&& other) = default;

	inline
	sexpr&
	operator=(const sexpr& other);

	sexpr&
	operator=(sexpr&& other) = default;

	inline
	const sexpr_terminal*
	as_terminal() const noexcept;

	inline
	const sexpr_compound*
	as_compound() const noexcept;

	inline
	void
	format(std::ostream& os) const;

	std::string
	debug_string() const;

	inline
	std::size_t
	location() const noexcept;

	inline
	static
	sexpr
	make_terminal(std::string value, std::size_t location);

	inline
	static
	sexpr
	make_compound(std::string kind, std::vector<sexpr> args, std::size_t location);

private:
	inline
	sexpr(std::unique_ptr<sexpr_repr> repr)
		: repr_(std::move(repr))
	{
	}

	std::unique_ptr<sexpr_repr> repr_;
};

class sexpr_repr {
public:
	virtual
	~sexpr_repr();

	inline explicit
	sexpr_repr(std::size_t location) : location_(location) {}

	virtual
	void
	format(std::ostream& os) const = 0;

	virtual
	std::unique_ptr<sexpr_repr>
	copy() const = 0;

	inline std::size_t
	location() const noexcept {
		return location_;
	}

private:
	std::size_t location_;
};

class sexpr_terminal final : public sexpr_repr {
public:
	~sexpr_terminal() override;

	inline
	sexpr_terminal(
		std::string value,
		std::size_t location
	) : sexpr_repr(location), value_(std::move(value)) {
	}

	void
	format(std::ostream& os) const override;

	std::unique_ptr<sexpr_repr>
	copy() const override;

	inline
	const std::string&
	value() const noexcept {
		return value_;
	}

private:
	std::string value_;
};

class sexpr_compound final : public sexpr_repr {
public:
	~sexpr_compound() override;

	inline
	sexpr_compound(
		std::string kind,
		std::vector<sexpr> args,
		std::size_t location
	) : sexpr_repr(location), kind_(std::move(kind)), args_(std::move(args)) {
	}

	void
	format(std::ostream& os) const override;

	std::unique_ptr<sexpr_repr>
	copy() const override;

	inline const std::string&
	kind() const noexcept {
		return kind_;
	}

	inline
	const std::vector<sexpr>&
	args() const noexcept {
		return args_;
	}

private:
	std::string kind_;
	std::vector<sexpr> args_;
};

inline
sexpr::sexpr(const sexpr& other) : repr_(other.repr_->copy()) {
}

inline
sexpr&
sexpr::operator=(const sexpr& other) {
	repr_ = other.repr_->copy();
	return *this;
}

inline
const sexpr_terminal*
sexpr::as_terminal() const noexcept {
	return dynamic_cast<const sexpr_terminal*>(repr_.get());
}

inline
const sexpr_compound*
sexpr::as_compound() const noexcept {
	return dynamic_cast<const sexpr_compound*>(repr_.get());
}

inline
void
sexpr::format(std::ostream& os) const {
	repr_->format(os);
}

inline
std::size_t
sexpr::location() const noexcept {
	return repr_->location();
}

inline
sexpr
sexpr::make_terminal(std::string value, std::size_t location) {
	return sexpr(std::make_unique<sexpr_terminal>(std::move(value), location));
}

inline
sexpr
sexpr::make_compound(std::string kind, std::vector<sexpr> args, std::size_t location) {
	return sexpr(std::make_unique<sexpr_compound>(std::move(kind), std::move(args), location));
}

}  // namespace coqcic

#endif  // COQCIC_SEXPR_H
