#include "coqcic/lazy_stackmap.h"

#include "gtest/gtest.h"

namespace coqcic {

TEST(LazyStackMap, small)
{
	using map_type = lazy_stackmap<std::string>;

	map_type m;
	m = m.push("c");
	m = m.push("a");
	m = m.push("b");
	m = m.push("a");

	ASSERT_TRUE(m.get_index("a"));
	ASSERT_EQ(0, *m.get_index("a"));
	ASSERT_TRUE(m.get_index("b"));
	ASSERT_EQ(1, *m.get_index("b"));
	ASSERT_TRUE(m.get_index("c"));
	ASSERT_EQ(3, *m.get_index("c"));

	m = m.push("c");
	ASSERT_EQ(0, *m.get_index("c"));
}

}  // namespace coqcic
