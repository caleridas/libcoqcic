#include "coqcic/simpl.h"

#include "coqcic/visitor.h"

namespace coqcic {

namespace {

class local_subst_visitor final : public transform_visitor {
public:
	~local_subst_visitor() override {}

	local_subst_visitor(
		std::size_t depth,
		std::size_t index,
		std::vector<constr_t> subst)
		: depth_(depth), index_(index), subst_(std::move(subst))
	{
	}

	void
	push_local(
		const std::string* name,
		const constr_t* type,
		const constr_t* value) override
	{
		++depth_;
	}

	void
	pop_local()
	{
		--depth_;
	}

	std::optional<constr_t>
	handle_local(const std::string& name, std::size_t index) override
	{
		if (index >= index_ + subst_.size() + depth_) {
			return {builder::local(name, index - subst_.size())};
		} else if (index >= index_ + depth_) {
			return {subst_[index - index_ - depth_].shift(0, depth_)};
		} else {
			return {};
		}
	}

private:
	std::size_t depth_;
	std::size_t index_;
	std::vector<constr_t> subst_;
};

}  // namespace

constr_t
local_subst(
	const constr_t& input,
	std::size_t index,
	std::vector<constr_t> subst)
{
	local_subst_visitor visitor(0, index, std::move(subst));
	auto result = visit_transform(input, visitor);

	if (result) {
		return std::move(*result);
	} else {
		return input;
	}
}

}  // namespace coqcic
