#include "coqcic/minigallina.h"

#include "gtest/gtest.h"

namespace coqcic {

TEST(minigallina_test, constr_parse) {
	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "Coq.Init.Datatypes.nat") {
			return constr_builtin::set();
		} else if (s == "nat") {
			return constr_builtin::set();
		} else if (s == "O") {
			return constr_global::create("nat");
		} else if (s == "S") {
			return constr_product::create({{{}, constr_global::create("nat")}}, constr_global::create("nat"));
		} else if (s == "Y") {
			return constr_builtin::set();
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "nat") {
				return coqcic::one_inductive_t {
					"nat",
					constr_builtin::set(),
					{
						{"O", constr_global::create("nat")},
						{"S", constr_product::create({{{}, constr_global::create("nat")}}, constr_global::create("nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	EXPECT_EQ(
		constr_builtin::prop(),
		coqcic::mgl::parse_constr("Prop", {}, {}).value());

	EXPECT_EQ(
		constr_global::create("Coq.Init.Datatypes.nat"),
		coqcic::mgl::parse_constr("Coq.Init.Datatypes.nat", globals_resolve, {}).value()
	);

	EXPECT_EQ(
		constr_let::create("x", constr_global::create("O"), constr_global::create("nat"), constr_local::create("x", 0)),
		coqcic::mgl::parse_constr("let x : nat := O in x", globals_resolve, inductive_resolve).value());

	EXPECT_EQ(
		constr_apply::create(constr_global::create("S"), {constr_global::create("O")}),
		coqcic::mgl::parse_constr("S O", globals_resolve, inductive_resolve).value());

	EXPECT_EQ(
		constr_let::create("x", constr_apply::create(constr_global::create("S"), {constr_global::create("O")}), constr_global::create("nat"), constr_local::create("x", 0)),
		coqcic::mgl::parse_constr("let x : nat := S O in x", globals_resolve, inductive_resolve).value());

	auto c = coqcic::mgl::parse_constr(
		"match O as _ return nat with "
		"  | O => S O "
		"  | S x => S (S x) "
		"end", globals_resolve, inductive_resolve).value();
	EXPECT_EQ(
		constr_match::create(
			constr_lambda::create({{"_", constr_global::create("nat")}}, constr_global::create("nat")),
			constr_global::create("O"),
			{
				{"O", 0, constr_apply::create(constr_global::create("S"), {constr_global::create("O")})},
				{"S", 1, constr_lambda::create({{"x", constr_global::create("nat")}}, constr_apply::create(constr_global::create("S"),{constr_apply::create(constr_global::create("S"), {constr_local::create("x", 0)})}))}
			}),
		c);

	EXPECT_EQ(
		constr_product::create({{"x", constr_global::create("nat")}}, constr_global::create("nat")),
		coqcic::mgl::parse_constr("forall (x : nat), nat", globals_resolve, inductive_resolve).value());
}

TEST(minigallina_test, constr_fix_parse) {
	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "Coq.Init.Datatypes.nat") {
			return constr_builtin::set();
		} else if (s == "nat") {
			return constr_builtin::set();
		} else if (s == "O") {
			return constr_global::create("nat");
		} else if (s == "S") {
			return constr_product::create({{{}, constr_global::create("nat")}}, constr_global::create("nat"));
		} else if (s == "Y") {
			return constr_builtin::set();
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "nat") {
				return coqcic::one_inductive_t {
					"nat",
					constr_builtin::set(),
					{
						{"O", constr_global::create("nat")},
						{"S", constr_product::create({{{}, constr_global::create("nat")}}, constr_global::create("nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	static const char FIX_EXPR[] =
		"fun (T : Set) => \n"
		"(fix f (n : nat) (t : T) : T := \n"
		"	match n as _ return nat with \n"
		"	| S n => g n t \n"
		"	| O => t \n"
		"	end \n"
		"with g (n : nat) (t : T) : T := \n"
		"	match n as _ return nat with \n"
		"	| S n => f n t \n"
		"	| O => t \n"
		"	end \n"
		"for f). \n";

	auto fix = coqcic::mgl::parse_constr(FIX_EXPR, globals_resolve, inductive_resolve);
	ASSERT_TRUE(fix) << fix.error().description << "@" << fix.error().location;
	EXPECT_EQ(
		"(T : Set => "
		"(fix f (n : nat) (t : T,3) : T,4 := match n,1 casetype (_ : nat => nat)| S 1 => (n : nat => (g,3 n,0 t,1))| O 0 => t,0 end "
		"with g (n : nat) (t : T,3) : T,4 := match n,1 casetype (_ : nat => nat)| S 1 => (n : nat => (f,4 n,0 t,1))| O 0 => t,0 end "
		"for f))",
		fix.value().debug_string()
	) << fix.value().debug_string();
}


TEST(minigallina_test, sfb_parse) {
	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "nat") {
			return constr_builtin::set();
		} else if (s == "O") {
			return constr_global::create("nat");
		} else if (s == "S") {
			return constr_product::create({{{}, constr_global::create("nat")}}, constr_global::create("nat"));
		} else if (s == "Y") {
			return constr_builtin::set();
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "nat") {
				return coqcic::one_inductive_t {
					"nat",
					constr_builtin::set(),
					{
						{"O", constr_global::create("nat")},
						{"S", constr_product::create({{{}, constr_global::create("nat")}}, constr_global::create("nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	EXPECT_EQ(
		sfb_definition::create("zero", constr_global::create("nat"), constr_global::create("O")),
		coqcic::mgl::parse_sfb("Definition zero : nat := O.", globals_resolve, inductive_resolve).value());

	EXPECT_EQ(
		sfb_inductive::create({
			{
				"nat",
				constr_builtin::set(),
				{
					{"O", constr_global::create("nat")},
					{"S", constr_product::create({{"x", constr_global::create("nat")}}, constr_global::create("nat"))}
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
		sfb_fixpoint::create(
			coqcic::fix_group_t {
				{
					coqcic::fix_function_t {
						"dup",
						{{"x", constr_global::create("nat")}},
						constr_global::create("nat"),
						constr_apply::create(constr_global::create("S"), {constr_apply::create(constr_local::create("dup", 1), {constr_local::create("x", 0)})} )
					}
				}
			}),
		coqcic::mgl::parse_sfb(
			"Fixpoint dup (x : nat) : nat := S (dup x).", globals_resolve, inductive_resolve
		).value()
	);

	EXPECT_EQ(
		sfb_inductive::create({
			{
				"list",
				constr_builtin::set(),
				{
					{"cons", constr_product::create({{"x", constr_global::create("nat")}, {"l", constr_global::create("list")}}, constr_global::create("list"))},
					{"nil", constr_global::create("list")}
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
		sfb_inductive::create({
			{
				"list",
				constr_product::create({{"T", constr_builtin::set()}}, constr_builtin::set()),
				{
					{
						"cons",
						constr_product::create(
							{
								{"T", constr_builtin::set()},
								{"x", constr_local::create("T", 0)},
								{"l", constr_apply::create(constr_global::create("list"), {{constr_local::create("T", 1)}})}
							},
							constr_apply::create(constr_global::create("list"), {{constr_local::create("T", 2)}})
						)
					},
					{
						"nil",
						constr_product::create(
							{
								{"T", constr_builtin::set()},
							},
							constr_apply::create(constr_global::create("list"), {{constr_local::create("T", 0)}})
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

TEST(minigallina_test, simple_module) {
	using namespace coqcic::builder;

	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "Coq.Init.Datatypes.nat") {
			return constr_builtin::set();
		} else if (s == "Coq.Init.Datatypes.O") {
			return constr_global::create("Coq.Init.Datatypes.nat");
		} else if (s == "Coq.Init.Datatypes.S") {
			return constr_product::create({{{}, constr_global::create("Coq.Init.Datatypes.nat")}}, constr_global::create("Coq.Init.Datatypes.nat"));
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "Coq.Init.Datatypes.nat") {
				return coqcic::one_inductive_t {
					"Coq.Init.Datatypes.nat",
					constr_builtin::set(),
					{
						{"O", constr_global::create("Coq.Init.Datatypes.nat")},
						{"S", constr_product::create({{{}, constr_global::create("Coq.Init.Datatypes.nat")}}, constr_global::create("Coq.Init.Datatypes.nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	auto mod = coqcic::mgl::parse_sfb(
		"Module root.\n"
		"Fixpoint plus (x : Coq.Init.Datatypes.nat) (y : Coq.Init.Datatypes.nat) : Coq.Init.Datatypes.nat :=\n"
		"  match x as _ return Coq.Init.Datatypes.nat with\n"
		"    | O => y \n"
		"    | S xx => Coq.Init.Datatypes.S (plus xx y) \n"
		"  end.\n"
		"End root.\n",
		globals_resolve,
		inductive_resolve
	).value();
}

TEST(minigallina_test, compound_module) {
	using namespace coqcic::builder;

	auto globals_resolve = [](const std::string& s) -> std::optional<coqcic::constr_t> {
		if (s == "Coq.Init.Datatypes.nat") {
			return constr_builtin::set();
		} else if (s == "Coq.Init.Datatypes.O") {
			return constr_global::create("Coq.Init.Datatypes.nat");
		} else if (s == "Coq.Init.Datatypes.S") {
			return constr_product::create({{{}, constr_global::create("Coq.Init.Datatypes.nat")}}, constr_global::create("Coq.Init.Datatypes.nat"));
		} else {
			return std::nullopt;
		}
	};

	auto inductive_resolve = [](const coqcic::constr_t& ind) -> std::optional<coqcic::one_inductive_t> {
		if (auto glob = ind.as_global()) {
			if (glob->name() == "Coq.Init.Datatypes.nat") {
				return coqcic::one_inductive_t {
					"Coq.Init.Datatypes.nat",
					constr_builtin::set(),
					{
						{"O", constr_global::create("Coq.Init.Datatypes.nat")},
						{"S", constr_product::create({{{}, constr_global::create("Coq.Init.Datatypes.nat")}}, constr_global::create("Coq.Init.Datatypes.nat"))}
					}
				};
			}
		}
		return std::nullopt;
	};

	auto parsed = coqcic::mgl::parse_sfb(
		"Module root.\n"
		"  Inductive pair (X : Set) (Y : Set) : Set :=\n"
		"    | make_pair : forall (x : X), forall (y : Y), pair\n"
		"  .\n"
		"  Definition swap_pair : forall (X : Set) (Y : Set) (p : root.pair X Y), root.pair Y X :=\n"
		"    fun (X : Set) (Y : Set) (p : root.pair X Y) => match p as _ return root.pair Y X with\n"
		"      | make_pair _ _ x y => root.pair Y X y x\n"
		"    end\n"
		"  .\n"
		"End root.\n",
		globals_resolve,
		inductive_resolve
	).value();

	auto mod = parsed.as_module();
	EXPECT_TRUE(mod);
}

}  // namespace coqcic
