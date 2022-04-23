#include "coqcic/shared_stack.h"

#include "gtest/gtest.h"

namespace coqcic {

namespace {

template<typename T>
std::vector<T>
to_vec(const shared_stack<T>& stack)
{
	std::vector<T> result;
	std::size_t size = stack.size();
	for (std::size_t n = 0; n < size; ++n) {
		result.push_back(stack.at(n));
	}

	return result;
}

}  // namespace

TEST(SharedStack, small)
{
	using stack_type = shared_stack<int>;

	stack_type s0;

	EXPECT_TRUE(s0.empty());

	auto s1 = s0.push(1);
	EXPECT_TRUE(s0.empty());
	EXPECT_FALSE(s1.empty());
	EXPECT_EQ(1, s1.size());
	EXPECT_EQ(1, s1.at(0));

	auto s2 = s1.push(2);
	EXPECT_EQ(2, s2.size());
	EXPECT_EQ(2, s2.at(0));
	EXPECT_EQ(1, s2.at(1));
}

TEST(SharedStack, push_pop)
{
	using stack_type = shared_stack<int>;
	using vec_type = std::vector<int>;

	stack_type s;
	EXPECT_EQ(to_vec(s), (vec_type{}));

	s = s.push(1);
	s = s.push(2);
	s = s.push(3);

	EXPECT_EQ(to_vec(s), (vec_type{3, 2, 1}));

	s = s.pop();

	EXPECT_EQ(to_vec(s), (vec_type{2, 1}));

	s = s.push(3);

	EXPECT_EQ(to_vec(s), (vec_type{3, 2, 1}));
}


TEST(SharedStack, large)
{
	using stack_type = shared_stack<int>;

	stack_type s;

	for (int n = 0; n < 1024; ++n) {
		s = s.push(1023 - n);
	}

	for (int n = 0; n < 1024; ++n) {
		EXPECT_EQ(n, s.at(n));
	}
}

TEST(SharedStack, assign)
{
	using stack_type = shared_stack<int>;

	stack_type s;
	s = s.push(1);
	s = s.push(2);
	s = s.push(3);

	EXPECT_EQ(3, s.at(0));
	EXPECT_EQ(2, s.at(1));
	EXPECT_EQ(1, s.at(2));

	stack_type copy = s;
	s.set(2, 42);

	EXPECT_EQ(3, s.at(0));
	EXPECT_EQ(2, s.at(1));
	EXPECT_EQ(42, s.at(2));

	EXPECT_EQ(3, copy.at(0));
	EXPECT_EQ(2, copy.at(1));
	EXPECT_EQ(1, copy.at(2));
}

}  // namespace coqcic
