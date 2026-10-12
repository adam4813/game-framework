#include <gtest/gtest.h>

class SanityCheckTest : public ::testing::Test {
protected:
	void SetUp() override {}

	void TearDown() override {}
};

TEST_F(SanityCheckTest, google_test_framework_works) {
	EXPECT_TRUE(true);
	EXPECT_EQ(1 + 1, 2);
	EXPECT_NE(0, 1);
}

TEST_F(SanityCheckTest, basic_math_operations_work) {
	int a = 5;
	int b = 3;

	EXPECT_EQ(a + b, 8);
	EXPECT_EQ(a - b, 2);
	EXPECT_EQ(a * b, 15);
	EXPECT_EQ(a / b, 1);
}

TEST_F(SanityCheckTest, string_operations_work) {
	std::string hello = "Hello";
	std::string world = "World";
	std::string combined = hello + " " + world;
	EXPECT_EQ(combined, "Hello World");
	EXPECT_EQ(hello.length(), 5);
	EXPECT_FALSE(world.empty());
}

TEST_F(SanityCheckTest, container_operations_work) {
	std::vector<int> numbers = {1, 2, 3, 4, 5};

	EXPECT_EQ(numbers.size(), 5);
	EXPECT_EQ(numbers[0], 1);
	EXPECT_EQ(numbers.back(), 5);

	numbers.push_back(6);
	EXPECT_EQ(numbers.size(), 6);
}
