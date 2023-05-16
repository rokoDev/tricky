#include <gtest/gtest.h>
#include <tricky/tricky.h>

#include "test_common.h"

namespace
{
using seq_t = cargo::seq<char, std::size_t>;
using cseq_t = cargo::seq<const char, std::size_t>;

class LazyLoadTest : public ::testing::Test
{
   protected:
    using e_source_location = tricky::e_source_location;
    using shard_state = tricky::shared_state;
    template <typename R, typename PH>
    std::enable_if_t<tricky::is_result_v<utils::remove_cvref_t<R>>,
                     typename utils::remove_cvref_t<R>::value_type>
    process_result(R&& aResult, PH&& aPayloadHandlers) noexcept
    {
        using return_type = typename utils::remove_cvref_t<R>::value_type;
        const auto process_error = tricky::handlers(
            tricky::handler([&aPayloadHandlers](auto) noexcept
                            { tricky::process_payload(aPayloadHandlers); }));

        if constexpr (std::is_same_v<return_type, void>)
        {
            process_error(std::move(aResult));
        }
        else
        {
            return process_error(std::move(aResult));
        }
    }
    void TearDown() override { shard_state::get_payload().reset(); }
};
}  // namespace
TEST_F(LazyLoadTest, TestWithoutError)
{
    bool is_payload_processed{};
    const auto payload_handlers =
        std::make_tuple([&is_payload_processed](cseq_t) noexcept
                        { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(cseq_t(aName, std::strlen(aName)));
        return result<void>{};
    };

    const auto r = make_result(kName);
    ASSERT_EQ(shard_state::get_const_payload().size(), 0);

    process_result(r, payload_handlers);
    ASSERT_FALSE(is_payload_processed);
}

TEST_F(LazyLoadTest, TestWithError)
{
    bool is_payload_processed{};
    const auto payload_handlers =
        std::make_tuple([&is_payload_processed](cseq_t) noexcept
                        { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(cseq_t(aName, std::strlen(aName)));
        return result<void>{eBufferError::kInvalidPointer};
    };

    const auto r = make_result(kName);
    ASSERT_EQ(shard_state::get_const_payload().size(), 1);

    process_result(r, payload_handlers);
    ASSERT_TRUE(is_payload_processed);
}

TEST_F(LazyLoadTest, TestWithErrorAndSourceLocation)
{
    bool is_payload_processed{};
    const auto payload_handlers = std::make_tuple(
        [&is_payload_processed](const e_source_location&, cseq_t) noexcept
        { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(cseq_t(aName, std::strlen(aName)));
        return result<void>{eBufferError::kInvalidPointer,
                            TRICKY_SOURCE_LOCATION};
    };

    const auto r = make_result(kName);
    ASSERT_EQ(shard_state::get_const_payload().size(), 2);

    process_result(r, payload_handlers);
    ASSERT_TRUE(is_payload_processed);
}

TEST_F(LazyLoadTest, TestWithErrorAndMovedLazyLoad)
{
    bool is_payload_processed{};
    const auto payload_handlers =
        std::make_tuple([&is_payload_processed](seq_t) noexcept
                        { is_payload_processed = true; });

    char const* kName = "name";
    auto make_result = [](char const* aName)
    {
        auto load = tricky::on_error(cseq_t(aName, std::strlen(aName)));
        return std::make_tuple(result<void>{eBufferError::kInvalidPointer},
                               std::move(load));
    };

    {
        auto [r, load] = make_result(kName);
        ASSERT_EQ(shard_state::get_const_payload().size(), 0);

        process_result(r, payload_handlers);
        ASSERT_FALSE(is_payload_processed);
    }
    ASSERT_EQ(shard_state::get_const_payload().size(), 0);
}

TEST_F(LazyLoadTest, TestWithLValue)
{
    int k = 5;
    auto load = tricky::on_error(k);
}
