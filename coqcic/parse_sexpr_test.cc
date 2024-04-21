#include "coqcic/parse_sexpr.h"

#include "gtest/gtest.h"

TEST(parse_sexpr_test, parse_success) {
	auto e = coqcic::parse_sexpr("(foo bar(baz bla   ))");
	EXPECT_TRUE(e);

	std::stringstream os;
	e.value().format(os);
	EXPECT_EQ(os.str(), "(foo bar (baz bla))");
}
