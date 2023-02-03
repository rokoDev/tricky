#include <gtest/gtest.h>
#include <tricky/tricky.h>

#include "test_common.h"

class LazyLoadTest : public ::testing::Test
{
   protected:
    using c_str = tricky::c_str;
    using e_source_location = tricky::e_source_location;
    using shard_state = tricky::shared_state;
    template <typename R, typename PH>
    std::enable_if_t<tricky::is_result_v<utils::remove_cvref_t<R>>, int>
    process_result(R&& aResult, PH&& aPayloadHandlers) noexcept
    {
        const auto process_error = tricky::handlers(tricky::handler(
            [&aPayloadHandlers](auto) noexcept
            {
                tricky::process_payload(aPayloadHandlers);
                return -3;
            }));

        return process_error(aResult);
    }
};

TEST_F(LazyLoadTest, TestWithoutError)
{
    bool is_payload_processed{};
    const auto payload_handlers = std::make_tuple(
        [&is_payload_processed](c_str) { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(c_str(aName));
        return result<void>{};
    };

    const auto r = make_result(kName);
    ASSERT_EQ(shard_state::get_payload().count(), 0);

    process_result(r, payload_handlers);
    ASSERT_FALSE(is_payload_processed);
}

TEST_F(LazyLoadTest, TestWithError)
{
    bool is_payload_processed{};
    const auto payload_handlers = std::make_tuple(
        [&is_payload_processed](c_str) { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(c_str(aName));
        return result<void>{eBufferError::kInvalidPointer};
    };

    const auto r = make_result(kName);
    ASSERT_EQ(shard_state::get_payload().count(), 1);

    process_result(r, payload_handlers);
    ASSERT_TRUE(is_payload_processed);
}

TEST_F(LazyLoadTest, TestWithErrorAndSourceLocation)
{
    bool is_payload_processed{};
    const auto payload_handlers =
        std::make_tuple([&is_payload_processed](const e_source_location&, c_str)
                        { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(c_str(aName));
        return result<void>{eBufferError::kInvalidPointer,
                            TRICKY_SOURCE_LOCATION};
    };

    const auto r = make_result(kName);
    ASSERT_EQ(shard_state::get_payload().count(), 2);

    process_result(r, payload_handlers);
    ASSERT_TRUE(is_payload_processed);
}

TEST_F(LazyLoadTest, TestWithErrorAndMovedLazyLoad)
{
    bool is_payload_processed{};
    const auto payload_handlers = std::make_tuple(
        [&is_payload_processed](c_str) { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(c_str(aName));
        return std::make_tuple(result<void>{eBufferError::kInvalidPointer},
                               std::move(load));
    };

    {
        auto [r, load] = make_result(kName);
        ASSERT_EQ(shard_state::get_payload().count(), 0);

        process_result(r, payload_handlers);
        ASSERT_FALSE(is_payload_processed);
    }
    ASSERT_EQ(shard_state::get_payload().count(), 0);
}
