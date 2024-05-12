#include "coqcic/minigallina.h"

#include "gtest/gtest.h"

TEST(minigallina_test, constr_parse) {
	using namespace coqcic::builder;

	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "Coq.Init.Datatypes.nat") {
			return builtin_set();
		} else if (s == "nat") {
			return builtin_set();
		} else if (s == "O") {
			return global("nat");
		} else if (s == "S") {
			return product({{{}, global("nat")}}, global("nat"));
		} else if (s == "Y") {
			return builtin_set();
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "nat") {
				return coqcic::one_inductive_t {
					"nat",
					builtin_set(),
					{
						{"O", global("nat")},
						{"S", product({{{}, global("nat")}}, global("nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	EXPECT_EQ(
		builtin_prop(),
		coqcic::mgl::parse_constr("Prop", {}, {}).value());

	EXPECT_EQ(
		global("Coq.Init.Datatypes.nat"),
		coqcic::mgl::parse_constr("Coq.Init.Datatypes.nat", globals_resolve, {}).value()
	);

	EXPECT_EQ(
		let("x", global("O"), global("nat"), local("x", 0)),
		coqcic::mgl::parse_constr("let x : nat := O in x", globals_resolve, inductive_resolve).value());

	EXPECT_EQ(
		apply(global("S"), {global("O")}),
		coqcic::mgl::parse_constr("S O", globals_resolve, inductive_resolve).value());

	EXPECT_EQ(
		let("x", apply(global("S"), {global("O")}), global("nat"), local("x", 0)),
		coqcic::mgl::parse_constr("let x : nat := S O in x", globals_resolve, inductive_resolve).value());

	auto c = coqcic::mgl::parse_constr(
		"match O as _ return nat with "
		"  | O => S O "
		"  | S x => S (S x) "
		"end", globals_resolve, inductive_resolve).value();
	EXPECT_EQ(
		match(
			product({{"_", global("nat")}}, global("nat")),
			global("O"),
			{
				{"O", 0, apply(global("S"), {global("O")})},
				{"S", 0, lambda({{"x", global("nat")}}, apply(global("S"),{apply(global("S"), {local("x", 0)})}))}
			}),
		c);

	EXPECT_EQ(
		product({{"x", global("nat")}}, global("nat")),
		coqcic::mgl::parse_constr("forall (x : nat), nat", globals_resolve, inductive_resolve).value());
}

TEST(minigallina_test, sfb_parse) {
	using namespace coqcic::builder;

	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "nat") {
			return builtin_set();
		} else if (s == "O") {
			return global("nat");
		} else if (s == "S") {
			return product({{{}, global("nat")}}, global("nat"));
		} else if (s == "Y") {
			return builtin_set();
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "nat") {
				return coqcic::one_inductive_t {
					"nat",
					builtin_set(),
					{
						{"O", global("nat")},
						{"S", product({{{}, global("nat")}}, global("nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	EXPECT_EQ(
		definition("zero", global("nat"), global("O")),
		coqcic::mgl::parse_sfb("Definition zero : nat := O.", globals_resolve, inductive_resolve).value());

	EXPECT_EQ(
		inductive({
			{
				"nat",
				builtin_set(),
				{
					{"O", global("nat")},
					{"S", product({{"x", global("nat")}}, global("nat"))}
				}
			}
		}),
		coqcic::mgl::parse_sfb(
			"Inductive nat : Set := \n"
			"  | O : nat \n"
			"  | S : forall (x : nat), nat\n"
			".", globals_resolve, inductive_resolve
		).value()
	);

	EXPECT_EQ(
		fixpoint(
			coqcic::fix_group_t {
				{
					coqcic::fix_function_t {
						"dup",
						{{"x", global("nat")}},
						global("nat"),
						apply(global("S"), {apply(local("dup", 1), {local("x", 0)})} )
					}
				}
			}),
		coqcic::mgl::parse_sfb(
			"Fixpoint dup (x : nat) : nat := S (dup x).", globals_resolve, inductive_resolve
		).value()
	);

	EXPECT_EQ(
		inductive({
			{
				"list",
				builtin_set(),
				{
					{"cons", product({{"x", global("nat")}, {"l", global("list")}}, global("list"))},
					{"nil", global("list")}
				}
			}
		}),
		coqcic::mgl::parse_sfb(
			"Inductive list : Set := \n"
			"  | cons : forall (x : nat), forall (l : list), list \n"
			"  | nil : list\n"
			".", globals_resolve, inductive_resolve
		).value()
	);

	EXPECT_EQ(
		inductive({
			{
				"list",
				product({{"T", builtin_set()}}, builtin_set()),
				{
					{
						"cons",
						product(
							{
								{"T", builtin_set()},
								{"x", local("T", 0)},
								{"l", apply(global("list"), {{local("T", 1)}})}
							},
							apply(global("list"), {{local("T", 2)}})
						)
					},
					{
						"nil",
						product(
							{
								{"T", builtin_set()},
							},
							apply(global("list"), {{local("T", 0)}})
						)
					}
				}
			}
		}),
		coqcic::mgl::parse_sfb(
			"Inductive list (T : Set) : Set := \n"
			"  | cons : forall (x : T), forall (l : list T), list T \n"
			"  | nil : list T\n"
			".", globals_resolve, inductive_resolve
		).value()
	);
}
