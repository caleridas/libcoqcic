#include "coqcic/normalize.h"

#include "gtest/gtest.h"

TEST(normalize_test, lambda_apply) {
	using namespace coqcic::builder;
	auto o = lambda(
		{{"a", global("nat")}},
		lambda(
			{{"b", global("nat")}},
			apply(apply(global("plus"), {local("a", 0)}), {local("b", 1)})));

	auto e = lambda(
		{{"a", global("nat")},{"b", global("nat")}},
		apply(global("plus"), {local("a", 0), local("b", 1)}));
	auto n = normalize(o);


	EXPECT_EQ(n, e);

	coqcic::type_context_t ctx;
	ctx.global_types = [](const std::string& sym) {
		if (sym == "plus") {
			return product({{"_", global("nat")}}, product({{"_", global("nat")}}, global("nat")));
		} else return builtin_type();
	};

	auto oc = o.check(ctx);
	std::cout << oc.debug_string() << "\n";
	auto ec = e.check(ctx);
	std::cout << ec.debug_string() << "\n";
}
