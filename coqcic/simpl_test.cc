#include "coqcic/simpl.h"

#include "gtest/gtest.h"

namespace coqcic {

namespace {

template<typename T>
std::vector<T>
to_vec(const lazy_stack<T>& stack) {
	std::vector<T> result;
	std::size_t size = stack.size();
	for (std::size_t n = 0; n < size; ++n) {
		result.push_back(stack.at(n));
	}

	return result;
}

}  // namespace

TEST(simpl_test, subst_0) {
	auto i = constr_lambda::create(
		{{"b", constr_global::create("nat")}},
		constr_apply::create(constr_global::create("plus"), {constr_local::create("b", 0), constr_local::create("a", 1)}));

	auto o = local_subst(i, 0, {constr_global::create("O")});
	auto e = constr_lambda::create(
		{{"b", constr_global::create("nat")}},
		constr_apply::create(constr_global::create("plus"), {constr_local::create("b", 0), constr_global::create("O")}));

	EXPECT_EQ(o, e);
}

TEST(simpl_test, subst_1) {
	auto i = constr_lambda::create(
		{{"b", constr_global::create("nat")}},
		constr_apply::create(constr_global::create("plus"), {constr_local::create("x", 2), constr_local::create("a", 1)}));

	auto o = local_subst(i, 0, {constr_global::create("O")});
	auto e = constr_lambda::create(
		{{"b", constr_global::create("nat")}},
		constr_apply::create(constr_global::create("plus"), {constr_local::create("x", 1), constr_global::create("O")}));

	EXPECT_EQ(o, e);
}

}  // namespace coqcic
