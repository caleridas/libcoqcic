#ifndef COQCIC_VISITOR_H
#define COQCIC_VISITOR_H

#include "coqcic/constr.h"

namespace coqcic {

// Interface for bottom-up visitor utility.
class transform_visitor {
public:
	virtual
	~transform_visitor();

	// Push local variable to stack.
	virtual void
	push_local(
		const std::string* name,
		const constr* type,
		const constr* value);

	// Remove local variable from stack again.
	virtual void
	pop_local();

	// Handler functions for constr kinds, called in bottom-up order (children
	// first). Each function can return a substitute for its input constr.
	// (It can also return none in which case the input is left untransformed).

	virtual
	std::optional<constr>
	handle_local(const std::string& name, std::size_t index);

	virtual
	std::optional<constr>
	handle_global(const std::string& name);

	virtual
	std::optional<constr>
	handle_builtin(const std::string& name);

	virtual
	std::optional<constr>
	handle_product(const std::vector<formal_arg>& args, const constr& restype);

	virtual
	std::optional<constr>
	handle_lambda(const std::vector<formal_arg>& args, const constr& body);

	virtual
	std::optional<constr>
	handle_let(const std::optional<std::string>& varname, const constr& value, const constr& body);

	virtual
	std::optional<constr>
	handle_apply(const constr& fn, const std::vector<constr>& args);

	virtual
	std::optional<constr>
	handle_case(const constr& restype, const constr& arg, const std::vector<case_repr::branch>& branches);

	virtual
	std::optional<constr>
	handle_fix(std::size_t index, const std::shared_ptr<const fix_group>& group);
};

std::optional<constr>
visit_transform(
	const constr& input,
	transform_visitor& visitor);

// Instantiates given Visitor with args... and applies it to transform the
// input. Returns transformed input (possibly identical to original input).
template<typename Visitor, typename... Args>
constr
visit_transform_simple(
	const constr& input,
	Args... args)
{
	Visitor visitor(std::forward<Args>(args)...);
	auto result = visit_transform(input, visitor);
	return result ? std::move(*result) : input;
}

}  // namespace coqcic

#endif  // COQCIC_VISITOR_H
