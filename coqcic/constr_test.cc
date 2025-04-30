#include "coqcic/constr.h"

#include "gtest/gtest.h"

namespace coqcic {

struct globals {
	constr_t nat, O, S, list, nil, prod, pair, type, set;
};

globals
build_globals() {
	struct globals globals;
	globals.nat = constr_global::create("nat");
	globals.O = constr_global::create("O");
	globals.S = constr_global::create("S");
	globals.list = constr_global::create("list");
	globals.nil= constr_global::create("nil");
	globals.prod = constr_global::create("prod");
	globals.pair = constr_global::create("pair");
	globals.type = constr_builtin::type();
	globals.set = constr_builtin::set();

	return globals;
}

constr_t
lookup_global(const globals& globals, const std::string& name) {
	if (name == "nat") {
		return globals.set;
	} else if (name == "O") {
		return globals.nat;
	} else if (name == "S") {
		return constr_product::create({{{}, globals.nat}}, globals.nat);
	} else if (name == "list") {
		return constr_product::create({{{}, globals.set}}, globals.set);
	} else if (name == "nil") {
		return constr_product::create({{"A", globals.set}}, constr_apply::create(globals.list, {constr_local::create("A", 0)}));
	} else if (name == "prod") {
		return constr_product::create({{{}, globals.set}, {{}, globals.set}}, globals.set);
	} else if (name == "pair") {
		return constr_product::create(
			{{"A", globals.set}, {"B", globals.set}, {{}, constr_local::create("A", 1)}, {{}, constr_local::create("B", 1)}},
			constr_apply::create(globals.prod, {constr_local::create("A", 3), constr_local::create("B", 2)})
		);
	} else {
		throw std::runtime_error("Unbound constr_global");
	}
}

TEST(constr_test, simple_check) {
	auto globals = build_globals();
	type_context_t ctx;
	ctx.global_types = [globals](const std::string& name) { return lookup_global(globals, name); };

	{
		auto e = constr_apply::create(globals.S, {globals.O});
		EXPECT_EQ(e.check(ctx), globals.nat);
		EXPECT_EQ(e.debug_string(), "(S O)");
	}

	{
		auto e = constr_apply::create(globals.nil, {globals.nat});
		EXPECT_EQ(e.check(ctx), constr_apply::create(globals.list, {globals.nat}));
		EXPECT_EQ(e.debug_string(), "(nil nat)");
	}
}

TEST(constr_test, dup_pair) {
	auto globals = build_globals();
	type_context_t ctx;
	ctx.global_types = [globals](const std::string& name) { return lookup_global(globals, name); };

	auto dup_pair = constr_lambda::create(
		{{"T", globals.set}, {"t", constr_local::create("T", 0)}},
		constr_apply::create(globals.pair, {constr_local::create("T", 1), constr_local::create("T", 1), constr_local::create("t", 0), constr_local::create("t", 0)})
	);

	auto dup_pair_type = constr_product::create(
		{{"T", globals.set}, {"t", constr_local::create("T", 0)}},
		constr_apply::create(globals.prod, {constr_local::create("T", 1), constr_local::create("T", 1)})
	);
	EXPECT_EQ(dup_pair.check(ctx), dup_pair_type);

	auto dup_nat = constr_apply::create(dup_pair, {globals.nat});

	auto dup_nat_simpl = constr_lambda::create(
		{{"t", globals.nat}},
		constr_apply::create(globals.pair, {globals.nat, globals.nat, constr_local::create("t", 0), constr_local::create("t", 0)})
	);

	EXPECT_EQ(dup_nat.simpl(), dup_nat_simpl);
	EXPECT_EQ(dup_nat.check(ctx),
		constr_product::create(
			{{"t", globals.nat}},
			constr_apply::create(globals.prod, {globals.nat, globals.nat})
		)
	);

	auto zero_zero = constr_apply::create(dup_nat, {globals.O});
	EXPECT_EQ(zero_zero.check(ctx), constr_apply::create(globals.prod, {globals.nat, globals.nat}));
}

}  // namespace coqcic
