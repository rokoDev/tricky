#include <gtest/gtest.h>
#include <tricky/context.h>
#include <tricky/payload.h>

#include "test_common.h"

using payload = tricky::payload<256, 16>;
using context =
    tricky::context<payload, eReaderError, eWriterError, eFileError>;

TEST(ContextTest, Constructor)
{
    payload p;
    context ctx(p);

    ASSERT_FALSE(ctx.is_error());
    ASSERT_FALSE(ctx.is_moved());
}

TEST(ContextTest, MoveConstructor)
{
    payload p;
    context ctx(p);
    context ctx2(std::move(ctx));

    ASSERT_FALSE(ctx.is_error());
    ASSERT_TRUE(ctx.is_moved());

    ASSERT_FALSE(ctx2.is_error());
    ASSERT_FALSE(ctx2.is_moved());
}

TEST(ContextTest, MoveAssignment)
{
    payload p;
    context ctx(p);
    context ctx2 = std::move(ctx);

    ASSERT_FALSE(ctx.is_error());
    ASSERT_TRUE(ctx.is_moved());

    ASSERT_FALSE(ctx2.is_error());
    ASSERT_FALSE(ctx2.is_moved());
}
