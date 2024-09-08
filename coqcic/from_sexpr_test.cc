#include "coqcic/from_sexpr.h"

#include "gtest/gtest.h"

#include "coqcic/parse_sexpr.h"

namespace {

static const char CONSTR_EXAMPLE[] = R"( (Prod
      (Name T)
      (Sort Set)
      (Prod
       (Name P)
       (Prod
        (Name m)
        (App
         (Global CPPSynth.export.List.mylist)
         (Local T 1))
        (Sort Type))
       (Prod
        (Name f)
        (App
         (Local P 1)
         (App
          (Global CPPSynth.export.List.my_nil)
          (Local T 2)))
        (Prod
         (Name f)
         (Prod
          (Name t)
          (Local T 3)
          (Prod
           (Name m)
           (App
            (Global CPPSynth.export.List.mylist)
            (Local T 4))
           (Prod
            (Anonymous)
            (App
             (Local P 4)
             (Local m 1))
            (App
             (Local P 5)
             (App
              (Global CPPSynth.export.List.mycons)
              (Local T 6)
              (Local t 3)
              (Local m 2))))))
         (Prod
          (Name m)
          (App
           (Global CPPSynth.export.List.mylist)
           (Local T 4))
          (App
           (Local P 4)
           (Local m 1))))))))";

}  // namespace

TEST(from_sexpr_test, parse_constr) {
	auto e = coqcic::parse_sexpr(CONSTR_EXAMPLE);
	ASSERT_TRUE(e) << e.error().description << ":" << e.error().location;
	auto r = constr_from_sexpr(e.value());
	ASSERT_TRUE(r);
	ASSERT_EQ(
		"(T : Set -> (P : (m : (CPPSynth.export.List.mylist T,1) -> Type) -> "
		"(f : (P,1 (CPPSynth.export.List.my_nil T,2)) -> "
		"(f : (t : T,3 -> (m : (CPPSynth.export.List.mylist T,4) -> "
		"((P,4 m,1) -> (P,5 (CPPSynth.export.List.mycons T,6 t,3 m,2))))) -> "
		"(m : (CPPSynth.export.List.mylist T,4) -> (P,4 m,1))))))",
		r.value().debug_string()
	);
}

namespace {

static const char SFB_INDUCTIVE_EXAMPLE[] = R"(
    (Inductive
     (OneInductive mylist
      (Prod
       (Name T)
       (Sort Set)
       (Sort Set))
      (Constructor my_nil
       (Prod
        (Name T)
        (Sort Set)
        (App
         (Local mylist 2)
         (Local T 1))))
      (Constructor mycons
       (Prod
        (Name T)
        (Sort Set)
        (Prod
         (Anonymous)
         (Local T 1)
         (Prod
          (Anonymous)
          (App
           (Local mylist 3)
           (Local T 2))
          (App
           (Local mylist 4)
           (Local T 3)))))))))";

}  // namespace

TEST(from_sexpr_test, parse_sfb_inductive) {
	auto e = coqcic::parse_sexpr(SFB_INDUCTIVE_EXAMPLE);
	ASSERT_TRUE(e) << e.error().description << ":" << e.error().location;
	auto s = sfb_from_sexpr(e.value());
	ASSERT_TRUE(s);
	ASSERT_EQ(
		s.value().debug_string(),
		"Inductive mylist : (T : Set -> Set) :=\n"
		" | my_nil : (T : Set -> (mylist,2 T,1))\n"
		" | mycons : (T : Set -> (T,1 -> ((mylist,3 T,2) -> (mylist,4 T,3))))\n"
		"."
	);
}

namespace {

static const char SFB_MODULE_EXAMPLE[] = R"(
(Module
 CPPSynth.export.X
 (Struct
  (Untyped)
  (Body
   (ModuleType
    Y
    (Body (Axiom foo (Sort Set))))
   (Module
    Z
    (Struct
     (Untyped)
      (Functor
       CPPSynth.export.y
       CPPSynth.export.X.Y
       (Body
        (Definition
         q
         (Sort Type)
         (App (Global CPPSynth.export.y) (Global CPPSynth.export.y.foo)))))))
   (Module
    YY
    (Struct
     (Typed CPPSynth.export.X.Y)
     (Body
      (Definition
       foo
       (Sort Set) (Global Coq.Init.Datatypes.nat)))))
   (Module
    ZZ
    (Algebraic
     (Apply
      CPPSynth.export.X.Z
      CPPSynth.export.X.YY)))
   (ModuleType
    A
    (Functor
     CPPSynth.export.y
     CPPSynth.export.X.Y
     (Body
      (Axiom
       f
       (Prod (Anonymous) (Global CPPSynth.export.y.foo) (Global CPPSynth.export.y.foo))))))
   (Module
    B
    (Struct
     (Typed
      (Functor
       CPPSynth.export.y
       CPPSynth.export.X.Y
       (Apply CPPSynth.export.X.A CPPSynth.export.y)))
     (Functor
      CPPSynth.export.y
      CPPSynth.export.X.Y
      (Body
       (Definition
        f
        (Prod
         (Name z)
         (Global CPPSynth.export.y.foo)
         (Global CPPSynth.export.y.foo))
        (Lambda
         (Name z)
         (Global CPPSynth.export.y.foo) (Local z 1))))))))))
)";

}  // namespace

TEST(from_sexpr_test, parse_sfb_module) {
	auto e = coqcic::parse_sexpr(SFB_MODULE_EXAMPLE);
	ASSERT_TRUE(e) << e.error().description << ":" << e.error().location;
	auto s = sfb_from_sexpr(e.value());
	ASSERT_TRUE(s) << s.error().description << ": " << s.error().context->debug_string() << "\n";
	ASSERT_EQ(
		s.value().debug_string(),
		"Module CPPSynth.export.X.\n"
		"ModuleType Y.\n"
		"Definition foo : Set.\n"
		"End.\n"
		"Module Z (CPPSynth.export.y : CPPSynth.export.X.Y).\n"
		"Definition q : Type := (CPPSynth.export.y CPPSynth.export.y.foo).\n"
		"End.\n"
		"Module YY.\n"
		"Definition foo : Set := Coq.Init.Datatypes.nat.\n"
		"End.\n"
		"Module ZZ := CPPSynth.export.X.Z CPPSynth.export.X.YY.\n"
		"ModuleType A (CPPSynth.export.y : CPPSynth.export.X.Y).\n"
		"Definition f : (CPPSynth.export.y.foo -> CPPSynth.export.y.foo).\n"
		"End.\n"
		"Module B (CPPSynth.export.y : CPPSynth.export.X.Y).\n"
		"Definition f : (z : CPPSynth.export.y.foo -> CPPSynth.export.y.foo) := (z : CPPSynth.export.y.foo => z,1).\n"
		"End.\n"
		"End.");
}

TEST(from_sexpr_test, parse_fix_expression) {
	static const char FIX_EXAMPLE[] = R"(
		(Fix
			0
			(Function
				(Name f)
				(Prod
					(Name n)
					(Global Coq.Init.Datatypes.nat)
					(Prod (Name t) (Local T 1) (Local T 2))
				)
				(Lambda
					(Name n)
					(Global Coq.Init.Datatypes.nat)
					(Lambda
						(Name t)
						(Local T 3)
						(Case
							0
							(Lambda
								(Name n)
								(Global Coq.Init.Datatypes.nat)
								(Local T 5)
							)
							(Match (Local n 1))
							(Branches
								(Branch
									Coq.Init.Datatypes.O
									0
									(Local t 0)
								)
								(Branch
									Coq.Init.Datatypes.S
									1
									(Lambda
										(Name n)
										(Global Coq.Init.Datatypes.nat)
										(App (Local g 3) (Local n 0) (Local t 1))
									)
								)
							)
						)
					)
				)
			)
			(Function
				(Name g)
				(Prod
					(Name n)
					(Global Coq.Init.Datatypes.nat)
					(Prod (Name t) (Local T 1) (Local T 2))
				)
				(Lambda
					(Name n)
					(Global Coq.Init.Datatypes.nat)
					(Lambda
						(Name t)
						(Local T 3)
						(Case
							0
							(Lambda
								(Name n)
								(Global Coq.Init.Datatypes.nat)
								(Local T 5)
							)
							(Match (Local n 1))
							(Branches
								(Branch
									Coq.Init.Datatypes.O
									0
									(Local t 0)
								)
								(Branch
									Coq.Init.Datatypes.S
									1
									(Lambda
										(Name n)
										(Global Coq.Init.Datatypes.nat)
										(App (Local f 4) (Local n 0) (Local t 1))
									)
								)
							)
						)
					)
				)
			)
		)
)";
	auto e = coqcic::parse_sexpr(FIX_EXAMPLE);
	ASSERT_TRUE(e) << e.error().description << ":" << e.error().location;
	auto fix = constr_from_sexpr(e.value());
	ASSERT_TRUE(fix) << fix.error().description;
	ASSERT_EQ(
		fix.value().debug_string(),
		"(fix f (n : Coq.Init.Datatypes.nat) (t : T,3) : T,4 := match n,1 casetype (n : Coq.Init.Datatypes.nat => T,5)"
		"| Coq.Init.Datatypes.O 0 => t,0"
		"| Coq.Init.Datatypes.S 1 => (n : Coq.Init.Datatypes.nat => (g,3 n,0 t,1)) "
		"end with g (n : Coq.Init.Datatypes.nat) (t : T,3) : T,4 := match n,1 casetype (n : Coq.Init.Datatypes.nat => T,5)"
		"| Coq.Init.Datatypes.O 0 => t,0"
		"| Coq.Init.Datatypes.S 1 => (n : Coq.Init.Datatypes.nat => (f,4 n,0 t,1)) "
		"end for f)"
	);

	auto as_fix = fix.value().as_fix();
	ASSERT_TRUE(as_fix);
	const auto& group = *as_fix->group();

	EXPECT_EQ(
		group.get_function_signature(0),
		coqcic::builder::product(
			{
				{
					"n",
					coqcic::builder::global("Coq.Init.Datatypes.nat")
				},
				{
					"t",
					coqcic::builder::local("T", 1)
				}
			},
			coqcic::builder::local("T", 2)
		)
	);
}
