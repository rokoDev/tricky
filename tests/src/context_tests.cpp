#include <cargo/cargo.h>
#include <gtest/gtest.h>
#include <tricky/context.h>

#include "test_common.h"

namespace
{
using namespace test_utils;
using payload = cargo::payload;
using context =
    tricky::heavy_context<payload, eReaderError, eWriterError, eFileError>;

template <std::size_t MaxSpace>
class Context : public ::testing::Test
{
   protected:
    static constexpr std::size_t kMaxSpace = MaxSpace;
    char buffer_[kMaxSpace]{};

    static bool memvcmp(const void *memptr, unsigned char val,
                        const std::size_t size) noexcept
    {
        if ((0 == size) || (nullptr == memptr))
        {
            return false;
        }
        const unsigned char *mm = static_cast<const unsigned char *>(memptr);
        return (*mm == val) && (memcmp(mm, mm + 1, size - 1) == 0);
    }
};

using ContextTest = Context<256>;
}  // namespace

TEST(CtxTest, StaticChecks)
{
    static_assert(not std::is_default_constructible_v<context>);
    static_assert(not std::is_copy_constructible_v<context>);
    static_assert(not std::is_copy_assignable_v<context>);
    static_assert(std::is_nothrow_destructible_v<context>);
    static_assert(std::is_nothrow_move_constructible_v<context>);
    static_assert(std::is_nothrow_move_assignable_v<context>);
}

TEST_F(ContextTest, Constructor)
{
    context ctx(buffer_);

    ASSERT_FALSE(ctx.has_error());
    ASSERT_TRUE(ctx.payload().data());
}

TEST_F(ContextTest, MoveConstructor)
{
    context ctx(buffer_);
    tricky::details::ctx::set_error(eFileError::kPermission, ctx);

    context ctx2(std::move(ctx));

    ASSERT_FALSE(ctx.payload().data());

    ASSERT_TRUE(ctx2.payload().data());
    ASSERT_TRUE(ctx2.has_error());
    ASSERT_TRUE(ctx2.has_error<eFileError>());
    ASSERT_EQ(ctx2.error().value<eFileError>(), eFileError::kPermission);

    tricky::details::ctx::reset_error(ctx2);
}

TEST_F(ContextTest, MoveAssignment)
{
    context ctx(buffer_);
    tricky::details::ctx::set_error(eFileError::kPermission, ctx);

    char buf2[32]{};
    context ctx2(buf2);
    ctx2 = std::move(ctx);

    ASSERT_FALSE(ctx.payload().data());

    ASSERT_TRUE(ctx2.payload().data());

    ASSERT_TRUE(ctx2.has_error());
    ASSERT_TRUE(ctx2.has_error<eFileError>());
    ASSERT_EQ(ctx2.error().value<eFileError>(), eFileError::kPermission);

    tricky::details::ctx::reset_error(ctx2);
}

TEST_F(ContextTest, HasError)
{
    context ctx(buffer_);
    ASSERT_FALSE(ctx.has_error());
    ASSERT_FALSE(ctx.has_error<eWriterError>());
    ASSERT_FALSE(ctx.has_error<eFileError>());

    tricky::details::ctx::set_error(eFileError::kPermission, ctx);

    ASSERT_TRUE(ctx.has_error());
    ASSERT_TRUE(ctx.has_error<eFileError>());
    ASSERT_EQ(ctx.error().value<eFileError>(), eFileError::kPermission);

    tricky::details::ctx::reset_error(ctx);

    ASSERT_FALSE(ctx.has_error());
    ASSERT_FALSE(ctx.has_error<eWriterError>());
    ASSERT_FALSE(ctx.has_error<eFileError>());
}
