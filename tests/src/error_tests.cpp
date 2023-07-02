#include <gtest/gtest.h>
#include <tricky/error.h>

#include "test_common.h"

namespace
{
using error = tricky::error;
}  // namespace

TEST(ErrorTest, CompileTimeChecks)
{
    static_assert(not std::is_default_constructible_v<error>);
    static_assert(not std::is_copy_constructible_v<error>);
    static_assert(not std::is_copy_assignable_v<error>);
    static_assert(std::is_nothrow_destructible_v<error>);
    static_assert(std::is_nothrow_move_constructible_v<error>);
    static_assert(std::is_nothrow_move_assignable_v<error>);
}

TEST(ErrorTest, Construct)
{
    error e(eReaderError::kError2);
    ASSERT_TRUE(e);
    ASSERT_TRUE(e.contains<eReaderError>());
    ASSERT_EQ(e.value<eReaderError>(), eReaderError::kError2);
    ASSERT_FALSE(e.contains<eWriterError>());
    ASSERT_EQ(e.type_name(), type_name::kName<eReaderError>);
}

TEST(ErrorTest, MoveConstructor)
{
    error e(eReaderError::kError2);
    auto e2(std::move(e));
    ASSERT_TRUE(e2);
    ASSERT_TRUE(e2.contains<eReaderError>());
    ASSERT_EQ(e2.value<eReaderError>(), eReaderError::kError2);
    ASSERT_FALSE(e2.contains<eWriterError>());
    ASSERT_EQ(e2.type_name(), type_name::kName<eReaderError>);

    ASSERT_FALSE(e);
    ASSERT_FALSE(e.contains<eReaderError>());
    ASSERT_EQ(e.type_name(), std::string_view());
}

TEST(ErrorTest, MoveAssignment)
{
    error e(eReaderError::kError2);
    auto e2 = std::move(e);
    ASSERT_TRUE(e2);
    ASSERT_TRUE(e2.contains<eReaderError>());
    ASSERT_EQ(e2.value<eReaderError>(), eReaderError::kError2);
    ASSERT_FALSE(e2.contains<eWriterError>());
    ASSERT_EQ(e2.type_name(), type_name::kName<eReaderError>);

    ASSERT_FALSE(e);
    ASSERT_FALSE(e.contains<eReaderError>());
    ASSERT_EQ(e.type_name(), std::string_view());
}
