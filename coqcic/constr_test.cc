#include "coqcic/constr.h"

#include "gtest/gtest.h"

using coqcic::constr;
using coqcic::type_context_t;
using namespace coqcic::builder;

namespace {

struct globals {
	constr nat, O, S, list, nil, prod, pair, type, set;
};

globals
build_globals()
{
	struct globals globals;
	globals.nat = global("nat");
	globals.O = global("O");
	globals.S = global("S");
	globals.list = global("list");
	globals.nil= global("nil");
	globals.prod = global("prod");
	globals.pair = global("pair");
	globals.type = builtin_type();
	globals.set = builtin_set();

	return globals;
}

coqcic::constr
lookup_global(const globals& globals, const std::string& name)
{
	if (name == "nat") {
		return globals.set;
	} else if (name == "O") {
		return globals.nat;
	} else if (name == "S") {
		return product({{{}, globals.nat}}, globals.nat);
	} else if (name == "list") {
		return product({{{}, globals.set}}, globals.set);
	} else if (name == "nil") {
		return product({{"A", globals.set}}, apply(globals.list, {local("A", 0)}));
	} else if (name == "prod") {
		return product({{{}, globals.set}, {{}, globals.set}}, globals.set);
	} else if (name == "pair") {
		return product(
			{{"A", globals.set}, {"B", globals.set}, {{}, local("A", 1)}, {{}, local("B", 1)}},
			apply(globals.prod, {local("A", 3), local("B", 2)})
		);
	} else {
		throw std::runtime_error("Unbound constr_global");
	}
}

}

TEST(CICTest, simple_check)
{
	auto globals = build_globals();
	type_context_t ctx;
	ctx.global_types = [globals](const std::string& name) { return lookup_global(globals, name); };

	{
		auto e = apply(globals.S, {globals.O});
		EXPECT_EQ(e.check(ctx), globals.nat);
		EXPECT_EQ(e.debug_string(), "(S O)");
	}

	{
		auto e = apply(globals.nil, {globals.nat});
		EXPECT_EQ(e.check(ctx), apply(globals.list, {globals.nat}));
		EXPECT_EQ(e.debug_string(), "(nil nat)");
	}
}

TEST(CICTest, dup_pair)
{
	auto globals = build_globals();
	type_context_t ctx;
	ctx.global_types = [globals](const std::string& name) { return lookup_global(globals, name); };

	auto dup_pair = lambda(
		{{"T", globals.set}, {"t", local("T", 0)}},
		apply(globals.pair, {local("T", 1), local("T", 1), local("t", 0), local("t", 0)})
	);

	auto dup_pair_type = product(
		{{"T", globals.set}, {"t", local("T", 0)}},
		apply(globals.prod, {local("T", 1), local("T", 1)})
	);
	EXPECT_EQ(dup_pair.check(ctx), dup_pair_type);

	auto dup_nat = apply(dup_pair, {globals.nat});

	auto dup_nat_simpl = lambda(
		{{"t", globals.nat}},
		apply(globals.pair, {globals.nat, globals.nat, local("t", 0), local("t", 0)})
	);

	EXPECT_EQ(dup_nat.simpl(), dup_nat_simpl);
	EXPECT_EQ(dup_nat.check(ctx),
		product(
			{{"t", globals.nat}},
			apply(globals.prod, {globals.nat, globals.nat})
		)
	);

	auto zero_zero = apply(dup_nat, {globals.O});
	EXPECT_EQ(zero_zero.check(ctx), apply(globals.prod, {globals.nat, globals.nat}));
}
