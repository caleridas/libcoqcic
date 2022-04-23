#include "coqcic/simpl.h"

#include "gtest/gtest.h"

namespace coqcic {

namespace {

template<typename T>
std::vector<T>
to_vec(const shared_stack<T>& stack)
{
	std::vector<T> result;
	std::size_t size = stack.size();
	for (std::size_t n = 0; n < size; ++n) {
		result.push_back(stack.at(n));
	}

	return result;
}

}  // namespace

TEST(SimplTest, subst_0)
{
	using namespace builder;
	auto i = lambda(
		{{"b", global("nat")}},
		apply(global("plus"), {local("b", 0), local("a", 1)}));

	auto o = local_subst(i, 0, {global("O")});
	auto e = lambda(
		{{"b", global("nat")}},
		apply(global("plus"), {local("b", 0), global("O")}));

	EXPECT_EQ(o, e);
}

TEST(SimplTest, subst_1)
{
	using namespace builder;
	auto i = lambda(
		{{"b", global("nat")}},
		apply(global("plus"), {local("x", 2), local("a", 1)}));

	auto o = local_subst(i, 0, {global("O")});
	auto e = lambda(
		{{"b", global("nat")}},
		apply(global("plus"), {local("x", 1), global("O")}));

	EXPECT_EQ(o, e);
}

}  // namespace coqcic
