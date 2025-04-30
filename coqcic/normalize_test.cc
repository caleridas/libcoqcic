#include "coqcic/normalize.h"

#include "gtest/gtest.h"

namespace coqcic {

TEST(normalize_test, lambda_apply) {
	auto o = constr_lambda::create(
		{{"a", constr_global::create("nat")}},
		constr_lambda::create(
			{{"b", constr_global::create("nat")}},
			constr_apply::create(constr_apply::create(constr_global::create("plus"), {constr_local::create("a", 0)}), {constr_local::create("b", 1)})));

	auto e = constr_lambda::create(
		{{"a", constr_global::create("nat")},{"b", constr_global::create("nat")}},
		constr_apply::create(constr_global::create("plus"), {constr_local::create("a", 0), constr_local::create("b", 1)}));
	auto n = normalize(o);


	EXPECT_EQ(n, e);

	coqcic::type_context_t ctx;
	ctx.global_types = [](const std::string& sym) {
		if (sym == "plus") {
			return constr_product::create(
				{{"_", constr_global::create("nat")}}, constr_product::create({{"_", constr_global::create("nat")}},
				constr_global::create("nat"))
			);
		} else {
			return constr_builtin::type();
		}
	};

	auto oc = o.check(ctx);
	std::cout << oc.debug_string() << "\n";
	auto ec = e.check(ctx);
	std::cout << ec.debug_string() << "\n";
}

}  // namespace coqcic
