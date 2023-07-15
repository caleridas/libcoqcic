#include "coqcic/fix_specialize.h"

#include "gtest/gtest.h"

using namespace coqcic::builder;

TEST(CICFixSpecTest, list_reverse)
{
	// This is the list "rev" function with signature:
	//   rev : (T : Type) -> (l : list T) -> list T
	//
	// First, we establish that we can consistently specialize this fixpoint
	// group on its first argument.
	coqcic::fix_group grp{
		{
			{
				"rev",
				{{"T", builtin_type()}, {"l", apply(global("list"), {local("T", 0)})}},
				apply(global("list"), {local("T", 1)}),
				match(
					apply(global("list"), {local("T", 2)}),
					local("l", 0),
					{
						{
							"nil", 0,
							apply(global("nil"), {local("T", 1)})
						},
						{
							"cons", 2,
							apply(
								global("app"),
								{
									local("T", 3),
									apply(local("rev", 4), {local("T", 3), local("l", 1)}),
									apply(global("cons"), {local("T", 3), local("x", 0), apply(global("nil"), {local("T", 3)})})
								}
							)
						}
					}
				)
			}
		}
	};

	std::vector<std::optional<std::size_t>> spec_args = {{0}, {}};
	auto spec = compute_fix_specialization_closure(grp, 0, spec_args);

	std::cout << grp.functions[0].body.debug_string() << "\n";

	EXPECT_TRUE(spec);
	EXPECT_EQ(1, spec->functions.size());
	EXPECT_EQ(spec_args, spec->functions[0].spec_args);

	auto new_grp = apply_fix_specialization(
		grp, *spec, {global("nat")}, [](std::size_t) -> std::string { return std::string("rev_nat"); });

	coqcic::fix_group expect_grp{
		{
			{
				"rev_nat",
				{{"l", apply(global("list"), {global("nat")})}},
				apply(global("list"), {global("nat")}),
				match(
					apply(global("list"), {global("nat")}),
					local("l", 0),
					{
						{
							"nil", 0,
							apply(global("nil"), {global("nat")})
						},
						{
							"cons", 2,
							apply(
								global("app"),
								{
									global("nat"),
									apply(local("rev_nat", 3), {local("l", 1)}),
									apply(global("cons"), {global("nat"), local("x", 0), apply(global("nil"), {global("nat")})})
								}
							)
						}
					}
				)
			}
		}
	};

	EXPECT_EQ(new_grp.functions[0].body, expect_grp.functions[0].body);

	for (const auto& fn : new_grp.functions) {
		std::cout << "fixfn " << fn.name << "\n";
		for (const auto& arg : fn.args) {
			std::cout << (arg.name ? *arg.name : "_");
			std::cout << ":" << arg.type.debug_string() << "\n";
		}
		std::cout << "-> " << fn.restype.debug_string() << "\n";
		std::cout << fn.body.debug_string() << "\n";
	}

	std::cout << expect_grp.functions[0].body.debug_string() << "\n";
}
