#include <cargo/cargo.h>
#include <gtest/gtest.h>
#include <tricky/context.h>

#include "test_common.h"

namespace
{
using payload = cargo::payload;
using context =
    tricky::context<payload, eReaderError, eWriterError, eFileError>;

template <std::size_t MaxSpace>
class Context : public ::testing::Test
{
   protected:
    static constexpr std::size_t kMaxSpace = MaxSpace;

    char raw_data_[kMaxSpace]{};
    payload p{raw_data_};

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

    void TearDown() override { p.reset(); }
};

using ContextTest = Context<256>;
}  // namespace

TEST_F(ContextTest, Constructor)
{
    context ctx(p);

    ASSERT_FALSE(ctx.is_error());
    ASSERT_FALSE(ctx.is_moved());
}

TEST_F(ContextTest, MoveConstructor)
{
    context ctx(p);
    context ctx2(std::move(ctx));

    ASSERT_FALSE(ctx.is_error());
    ASSERT_TRUE(ctx.is_moved());

    ASSERT_FALSE(ctx2.is_error());
    ASSERT_FALSE(ctx2.is_moved());
}

TEST_F(ContextTest, MoveAssignment)
{
    context ctx(p);
    context ctx2 = std::move(ctx);

    ASSERT_FALSE(ctx.is_error());
    ASSERT_TRUE(ctx.is_moved());

    ASSERT_FALSE(ctx2.is_error());
    ASSERT_FALSE(ctx2.is_moved());
}
