#include "coqcic/to_sexpr.h"

#include "gtest/gtest.h"

#include "coqcic/from_sexpr.h"
#include "coqcic/parse_sexpr.h"

namespace coqcic {

class to_sexpr_test : public ::testing::Test {
public:
	inline void
	validate_constr_roundtrip(const coqcic::constr_t& constr) const {
		coqcic::sexpr e = coqcic::constr_to_sexpr(constr);
		std::stringstream ss;
		e.format(ss);

		auto re = coqcic::parse_sexpr(ss.str());
		EXPECT_TRUE(re) << re.error().description << "@" << re.error().location << ":\n" << ss.str();
		if (!re) {
			return;
		}

		auto rc = constr_from_sexpr(re.value());
		EXPECT_TRUE(rc) << rc.error().description << "@" << rc.error().context->location() << ":\n" << ss.str();
		if (!rc) {
			return;
		}

		EXPECT_EQ(constr, rc.value()) << constr.debug_string() << "\nvs\n" << rc.value().debug_string();
	}
};

TEST_F(to_sexpr_test, parse_success) {
	validate_constr_roundtrip(
		constr_global::create("nat")
	);
	validate_constr_roundtrip(
		constr_let::create(
			"foo",
			constr_apply::create(constr_global::create("S"), {constr_global::create("O")}),
			constr_global::create("nat"), constr_apply::create(constr_global::create("S"), {constr_local::create("foo", 0)})
		)
	);
}

}  // namespace coqcic
