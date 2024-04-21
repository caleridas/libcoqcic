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
		const constr_t* type,
		const constr_t* value
	);

	// Remove local variable from stack again.
	virtual void
	pop_local();

	// Handler functions for constr_t kinds, called in bottom-up order (children
	// first). Each function can return a substitute for its input constr.
	// (It can also return none in which case the input is left untransformed).

	virtual
	std::optional<constr_t>
	handle_local(const std::string& name, std::size_t index);

	virtual
	std::optional<constr_t>
	handle_global(const std::string& name);

	virtual
	std::optional<constr_t>
	handle_builtin(const std::string& name);

	virtual
	std::optional<constr_t>
	handle_product(const std::vector<formal_arg_t>& args, const constr_t& restype);

	virtual
	std::optional<constr_t>
	handle_lambda(const std::vector<formal_arg_t>& args, const constr_t& body);

	virtual
	std::optional<constr_t>
	handle_let(const std::optional<std::string>& varname, const constr_t& value, const constr_t& type, const constr_t& body);

	virtual
	std::optional<constr_t>
	handle_apply(const constr_t& fn, const std::vector<constr_t>& args);

	virtual
	std::optional<constr_t>
	handle_cast(const constr_t& term, const constr_cast::kind_type kind, const constr_t& typeterm);

	virtual
	std::optional<constr_t>
	handle_match(const constr_t& casetype, const constr_t& arg, const std::vector<match_branch_t>& branches);

	virtual
	std::optional<constr_t>
	handle_fix(std::size_t index, const std::shared_ptr<const fix_group_t>& group);
};

std::optional<constr_t>
visit_transform(
	const constr_t& input,
	transform_visitor& visitor);

// Instantiates given Visitor with args... and applies it to transform the
// input. Returns transformed input (possibly identical to original input).
template<typename Visitor, typename... Args>
constr_t
visit_transform_simple(
	const constr_t& input,
	Args... args)
{
	Visitor visitor(std::forward<Args>(args)...);
	auto result = visit_transform(input, visitor);
	return result ? std::move(*result) : input;
}

}  // namespace coqcic

#endif  // COQCIC_VISITOR_H
