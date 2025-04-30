#include "coqcic/fix_specialize.h"

#include "gtest/gtest.h"

namespace coqcic {

TEST(fix_specialize_test, list_reverse) {
	// This is the list "rev" function with signature:
	//   rev : (T : Type) -> (l : list T) -> list T
	//
	// First, we establish that we can consistently specialize this fixpoint
	// group on its first argument.
	coqcic::fix_group_t grp{
		{
			{
				"rev",
				{{"T", constr_builtin::type()}, {"l", constr_apply::create(constr_global::create("list"), {constr_local::create("T", 0)})}},
				constr_apply::create(constr_global::create("list"), {constr_local::create("T", 1)}),
				constr_match::create(
					constr_apply::create(constr_global::create("list"), {constr_local::create("T", 2)}),
					constr_local::create("l", 0),
					{
						{
							"nil", 0,
							constr_apply::create(constr_global::create("nil"), {constr_local::create("T", 1)})
						},
						{
							"cons", 2,
							constr_apply::create(
								constr_global::create("app"),
								{
									constr_local::create("T", 3),
									constr_apply::create(constr_local::create("rev", 4), {constr_local::create("T", 3), constr_local::create("l", 1)}),
									constr_apply::create(constr_global::create("cons"), {constr_local::create("T", 3), constr_local::create("x", 0), constr_apply::create(constr_global::create("nil"), {constr_local::create("T", 3)})})
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
		grp, *spec, {constr_global::create("nat")}, [](std::size_t) -> std::string { return std::string("rev_nat"); });

	coqcic::fix_group_t expect_grp{
		{
			{
				"rev_nat",
				{{"l", constr_apply::create(constr_global::create("list"), {constr_global::create("nat")})}},
				constr_apply::create(constr_global::create("list"), {constr_global::create("nat")}),
				constr_match::create(
					constr_apply::create(constr_global::create("list"), {constr_global::create("nat")}),
					constr_local::create("l", 0),
					{
						{
							"nil", 0,
							constr_apply::create(constr_global::create("nil"), {constr_global::create("nat")})
						},
						{
							"cons", 2,
							constr_apply::create(
								constr_global::create("app"),
								{
									constr_global::create("nat"),
									constr_apply::create(constr_local::create("rev_nat", 3), {constr_local::create("l", 1)}),
									constr_apply::create(constr_global::create("cons"), {constr_global::create("nat"), constr_local::create("x", 0), constr_apply::create(constr_global::create("nil"), {constr_global::create("nat")})})
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

}  // namespace coqcic
