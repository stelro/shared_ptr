#include <gtest/gtest.h>

#include "shared_ptr.hpp"

TEST(SharedPtr, BasicAssertion) {
    EXPECT_EQ(2 + 2, 4);
}

TEST(SharedPtr, CanCreateAPointer) {
	stel::shared_ptr<int> ptr = stel::make_shared<int>(42);
	EXPECT_NE(ptr.get(), nullptr);
	EXPECT_EQ(*ptr.get(), 42);
}

TEST(SharedPtr, PointerHasDerefereceOperator) {
	stel::shared_ptr<int> ptr = stel::make_shared<int>(42);
	//EXPECT_NE(ptr, nullptr);
	//EXPECT_EQ(*ptr, 42);
}
