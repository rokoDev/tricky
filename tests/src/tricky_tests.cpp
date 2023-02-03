#include <gtest/gtest.h>
#include <tricky/tricky.h>
#include <user_literals/user_literals.h>

#include "test_common.h"

class TrickyTest : public ::testing::Test
{
   protected:
    static constexpr auto handle_any_error =
        tricky::handlers(tricky::handler([](auto) noexcept { return -1; }));
};

class TrickyHandlersTest : public ::testing::Test
{
   protected:
    enum class e_payload
    {
        k_none = 0,
        k_char,
        k_float_char,
        k_float_char_uint32_t,
        k_src_location
    };
    template <typename R>
    std::enable_if_t<tricky::is_result_v<utils::remove_cvref_t<R>>, int>
    process_result(R&& aResult) noexcept
    {
        const auto payload_handlers = std::make_tuple(
            [this](char) { payload_flag_ = e_payload::k_char; },
            [this](float, char) { payload_flag_ = e_payload::k_float_char; },
            [this](float, char, uint32_t)
            { payload_flag_ = e_payload::k_float_char_uint32_t; },
            [this](const tricky::e_source_location&)
            { payload_flag_ = e_payload::k_src_location; });

        const auto process_error = tricky::handlers(
            tricky::handler<eFileError>(
                [&payload_handlers](auto) noexcept
                {
                    tricky::process_payload(payload_handlers);
                    return -1;
                }),
            tricky::handler<eReaderError::kError2, eFileError::kPermission>(
                [&payload_handlers](auto) noexcept
                {
                    tricky::process_payload(payload_handlers);
                    return -2;
                }),
            tricky::handler(
                [&payload_handlers](auto) noexcept
                {
                    tricky::process_payload(payload_handlers);
                    return -3;
                }));

        return process_error(aResult);
    }

    e_payload payload_flag_{};
};

TEST_F(TrickyTest, DefaultResultConstructor)
{
    using Result = result<uint8_t>;
    constexpr Result r;
    ASSERT_FALSE(r.has_error());
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r.value(), 0_u8);
    static_assert(std::is_same_v<Result::value_type, uint8_t>,
                  "invalid value_type");
}

TEST_F(TrickyTest, ConstructorWithValue)
{
    constexpr int8_t kValue = static_cast<int8_t>(-10);
    constexpr result<int8_t> r(kValue);
    ASSERT_FALSE(r.has_error());
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r.value(), kValue);
}

TEST_F(TrickyTest, ConstructorWithError)
{
    constexpr eWriterError kError = eWriterError::kError4;
    const result<int8_t> r{kError};
    ASSERT_TRUE(r.has_error());
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, ConstructorVoidResultWithError)
{
    constexpr eWriterError kError = eWriterError::kError4;
    const result<void> r{kError};
    ASSERT_TRUE(r.has_error());
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    static_assert(std::is_same_v<result<void>::value_type, void>,
                  "invalid value_type");
    handle_any_error(r);
}

TEST_F(TrickyTest, CallErrorForConstRValue)
{
    const result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(std::move(r).error<eBufferError>(),
              eBufferError::kInvalidPointer);
    handle_any_error(r);
}

TEST_F(TrickyTest, CallErrorForRValue)
{
    result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(std::move(r).error<eBufferError>(),
              eBufferError::kInvalidPointer);
    handle_any_error(r);
}

TEST_F(TrickyTest, CallErrorForConstRef)
{
    const result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    handle_any_error(r);
}

TEST_F(TrickyTest, CallErrorForRef)
{
    result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    handle_any_error(r);
}

TEST_F(TrickyTest, ConversionFromBufferResult)
{
    buffer::result<void> bufResult(eBufferError::kInvalidPointer);
    result<void> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eBufferError>());
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    handle_any_error(r);
}

TEST_F(TrickyTest, ConversionFromWriterResult)
{
    writer::result<int16_t> bufResult(eWriterError::kError4);
    result<int16_t> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, ConstConversion)
{
    const buffer::result<int> bufResult(eBufferError::kInvalidPointer);
    const result<int> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eBufferError>());
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    handle_any_error(r);
}

TEST_F(TrickyTest, ConversionFromWriterResultWithValue)
{
    constexpr int16_t kValue = static_cast<int16_t>(-16181);
    writer::result<int16_t> bufResult(kValue);
    result<int16_t> r(std::move(bufResult));
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r.is_active_type<int16_t>());
    ASSERT_EQ(r.value(), kValue);
}

TEST_F(TrickyTest, Conversion1)
{
    writer::result<void> bufResult(eWriterError::kError4);
    result<int16_t> r(bufResult);
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, Conversion2)
{
    writer::result<void> bufResult(eWriterError::kError4);
    result<int16_t> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, Conversion3)
{
    writer::result<int16_t> bufResult(eWriterError::kError4);
    result<void> r(bufResult);
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, Conversion4)
{
    writer::result<int16_t> bufResult(eWriterError::kError4);
    result<void> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, Conversion5)
{
    writer::result<void> bufResult(eWriterError::kError4);
    result<void> r(bufResult);
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    handle_any_error(r);
}

TEST_F(TrickyTest, Conversion6)
{
    writer::result<void> bufResult{};
    ASSERT_TRUE(bufResult.is_active_type<void>());
    result<void> r(bufResult);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r.is_active_type<void>());
}

TEST_F(TrickyTest, OneErrorValueHandlerWithoutArguments)
{
    eReaderError activeError{};
    const auto handle_result =
        tricky::handlers(tricky::handler<eReaderError::kError2>(
            [&activeError]() noexcept
            {
                activeError = eReaderError::kError2;
                return -1;
            }));

    reader::result<int> r = eReaderError::kError2;
    handle_result(r);
    ASSERT_EQ(activeError, eReaderError::kError2);

    r = eReaderError::kError1;
    handle_result(r);
    ASSERT_EQ(activeError, eReaderError::kError2);
    handle_any_error(r);
}

TEST_F(TrickyTest, OneErrorValueHandlerWithArguments)
{
    eReaderError activeError{};
    const auto handle_result =
        tricky::handlers(tricky::handler<eReaderError::kError2>(
            [&activeError]([[maybe_unused]] auto aError) noexcept
            {
                activeError = eReaderError::kError2;
                return -1;
            }));

    reader::result<int> r = eReaderError::kError2;
    handle_result(r);
    ASSERT_EQ(activeError, eReaderError::kError2);

    r = eReaderError::kError1;
    handle_result(r);
    ASSERT_EQ(activeError, eReaderError::kError2);
    handle_any_error(r);
}

TEST_F(TrickyTest, HandleTwoErrorValues)
{
    bool is_processed{false};
    const auto process_error = tricky::handlers(
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_processed]([[maybe_unused]] auto aError) noexcept
            {
                is_processed = true;
                return -1;
            }));

    result<int> r = eReaderError::kError2;
    process_error(r);
    ASSERT_TRUE(is_processed);

    is_processed = false;
    r = eFileError::kPermission;
    process_error(r);
    ASSERT_TRUE(is_processed);
}

TEST_F(TrickyTest, HandleTwoErrorValuesAndOneCategory)
{
    bool is_value_processed{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>(
            [&file_error](auto aError) noexcept
            {
                file_error = aError;
                return -1;
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed]([[maybe_unused]] auto aError) noexcept
            {
                is_value_processed = true;
                return -1;
            }));

    result<int> r = eReaderError::kError2;
    process_error(r);
    ASSERT_TRUE(is_value_processed);
    ASSERT_EQ(file_error, eFileError::kOpenError);

    is_value_processed = false;
    r = eFileError::kPermission;
    process_error(r);
    ASSERT_TRUE(is_value_processed);
    ASSERT_EQ(file_error, eFileError::kOpenError);

    is_value_processed = false;
    r = eFileError::kAccessDenied;
    process_error(r);
    ASSERT_EQ(file_error, eFileError::kAccessDenied);
    ASSERT_FALSE(is_value_processed);
}

TEST_F(TrickyTest, IntHandlersForTwoErrorValuesForOneCategoryAndForRest)
{
    bool is_value_processed{false};
    bool is_handler_for_any_error_called{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>(
            [&file_error](auto aError) noexcept
            {
                file_error = aError;
                return -1;
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed]([[maybe_unused]] auto aError) noexcept
            {
                is_value_processed = true;
                return -1;
            }),
        tricky::handler(
            [&is_handler_for_any_error_called](
                [[maybe_unused]] auto aError) noexcept
            {
                is_handler_for_any_error_called = true;
                return -1;
            }));

    result<int> r = eReaderError::kError2;
    static_assert(sizeof(result<int>) == sizeof(int),
                  "sizeof(result<T>) must be equal to sizeof(T)");
    process_error(r);
    ASSERT_TRUE(is_value_processed);
    ASSERT_EQ(file_error, eFileError::kOpenError);
    ASSERT_FALSE(is_handler_for_any_error_called);

    is_value_processed = false;
    r = eFileError::kPermission;
    process_error(r);
    ASSERT_TRUE(is_value_processed);
    ASSERT_EQ(file_error, eFileError::kOpenError);
    ASSERT_FALSE(is_handler_for_any_error_called);

    is_value_processed = false;
    r = eFileError::kAccessDenied;
    process_error(r);
    ASSERT_EQ(file_error, eFileError::kAccessDenied);
    ASSERT_FALSE(is_value_processed);
    ASSERT_FALSE(is_handler_for_any_error_called);

    file_error = eFileError::kOpenError;
    r = eBufferError::kInvalidPointer;
    process_error(r);
    ASSERT_EQ(file_error, eFileError::kOpenError);
    ASSERT_FALSE(is_value_processed);
    ASSERT_TRUE(is_handler_for_any_error_called);
}

TEST_F(TrickyTest, VoidHandlersForTwoErrorValuesForOneCategoryAndForRest)
{
    bool is_value_processed{false};
    bool is_handler_for_any_error_called{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>(
            [&file_error](auto aError) noexcept
            {
                file_error = aError;
                return -1;
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed]([[maybe_unused]] auto aError) noexcept
            {
                is_value_processed = true;
                return -1;
            }),
        tricky::handler(
            [&is_handler_for_any_error_called](
                [[maybe_unused]] auto aError) noexcept
            {
                is_handler_for_any_error_called = true;
                return -1;
            }));

    result<void> r = eReaderError::kError2;
    process_error(r);
    ASSERT_TRUE(is_value_processed);
    ASSERT_EQ(file_error, eFileError::kOpenError);
    ASSERT_FALSE(is_handler_for_any_error_called);

    is_value_processed = false;
    r = eFileError::kPermission;
    process_error(r);
    ASSERT_TRUE(is_value_processed);
    ASSERT_EQ(file_error, eFileError::kOpenError);
    ASSERT_FALSE(is_handler_for_any_error_called);

    is_value_processed = false;
    r = eFileError::kAccessDenied;
    process_error(r);
    ASSERT_EQ(file_error, eFileError::kAccessDenied);
    ASSERT_FALSE(is_value_processed);
    ASSERT_FALSE(is_handler_for_any_error_called);

    file_error = eFileError::kOpenError;
    r = eBufferError::kInvalidPointer;
    process_error(r);
    ASSERT_EQ(file_error, eFileError::kOpenError);
    ASSERT_FALSE(is_value_processed);
    ASSERT_TRUE(is_handler_for_any_error_called);
}

TEST_F(TrickyTest, ReturnErrorFromSubcall)
{
    const auto spawn_error = []() -> result<int>
    {
        result<int> r(eFileError::kSystemError);
        return r;
    };

    const auto clients_code = [spawn_error]()
    {
        auto r = spawn_error();
        return r;
    };

    const auto r = clients_code();
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eFileError>());
    ASSERT_EQ(r.error<eFileError>(), eFileError::kSystemError);
    handle_any_error(r);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload1)
{
    result<void> r = eReaderError::kError2;
    ASSERT_EQ(process_result(r), -2);
    ASSERT_EQ(payload_flag_, e_payload::k_none);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload2)
{
    result<void> r = eFileError::kPermission;
    r.load('j');
    ASSERT_EQ(process_result(r), -2);
    ASSERT_EQ(payload_flag_, e_payload::k_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload3)
{
    result<int> r = eFileError::kPermission;
    r.load(1.23f);
    r.load('j');
    ASSERT_EQ(process_result(r), -2);
    ASSERT_EQ(payload_flag_, e_payload::k_float_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload4)
{
    result<int> r = eFileError::kSystemError;
    r.load(1.23f);
    r.load('j');
    ASSERT_EQ(process_result(r), -1);
    ASSERT_EQ(payload_flag_, e_payload::k_float_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload5)
{
    network::result<int> r = eNetworkError::kLostConnection;
    r.load(1.23f);
    r.load('j');
    r.load(3456_u32);
    ASSERT_EQ(process_result(r), -3);
    ASSERT_EQ(payload_flag_, e_payload::k_float_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload6)
{
    network::result<int> r = eNetworkError::kUnreachableHost;
    r.load('j');
    r.load(3456_u32);
    ASSERT_EQ(process_result(r), -3);
    ASSERT_EQ(payload_flag_, e_payload::k_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload7)
{
    result<int> r = eFileError::kBusyDescriptor;
    r.load('j');
    r.load(3456_u32);
    ASSERT_EQ(process_result(r), -1);
    ASSERT_EQ(payload_flag_, e_payload::k_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload8)
{
    result<int> r = 10;
    ASSERT_EQ(process_result(r), 0);
    ASSERT_EQ(payload_flag_, e_payload::k_none);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload9)
{
    result<int> r = TRICKY_NEW_ERROR(eFileError::kBusyDescriptor);
    ASSERT_EQ(process_result(r), -1);
    ASSERT_EQ(payload_flag_, e_payload::k_src_location);
}

TEST(TrickySimpleTest, HandleErrorForWhichHandlerDoesNotProvided)
{
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError::kEOF>([](auto) noexcept { return -2; }));
    result<int> r = TRICKY_NEW_ERROR(eFileError::kPermission);
    ASSERT_EQ(process_error(r), -1);

    const auto swallow_error =
        tricky::handlers(tricky::handler([](auto) noexcept { return -1; }));
    swallow_error(r);
}

TEST(TrickySimpleTest, LoadToPayloadMultipleValuesViaConstructor)
{
    bool isPayloadProcessed{};
    const auto payload_handlers = std::make_tuple(
        [&isPayloadProcessed](const tricky::e_source_location&, tricky::c_str)
        { isPayloadProcessed = true; });

    const auto process_error = tricky::handlers(tricky::handler(
        [&payload_handlers](auto) noexcept
        {
            tricky::process_payload(payload_handlers);
            return -2;
        }));

    const char* fileName = "myfile.txt";
    result<int> r{eFileError::kPermission, TRICKY_SOURCE_LOCATION,
                  tricky::c_str(fileName)};
    process_error(r);
    ASSERT_TRUE(isPayloadProcessed);
}

TEST(TrickySimpleTest, LoadToPayloadMultipleValuesViaLoad)
{
    bool isPayloadProcessed{};
    const auto payload_handlers = std::make_tuple(
        [&isPayloadProcessed](const tricky::e_source_location&, tricky::c_str)
        { isPayloadProcessed = true; });

    const auto process_error = tricky::handlers(tricky::handler(
        [&payload_handlers](auto) noexcept
        {
            tricky::process_payload(payload_handlers);
            return -2;
        }));

    const char* fileName = "myfile.txt";
    result<int> r{eFileError::kPermission};
    r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
    process_error(r);
    ASSERT_TRUE(isPayloadProcessed);
}

TEST(TrickySimpleTest, LoadToPayloadMultipleValuesViaConstructorOfVoidResult)
{
    bool isPayloadProcessed{};
    const auto payload_handlers = std::make_tuple(
        [&isPayloadProcessed](const tricky::e_source_location&, tricky::c_str)
        { isPayloadProcessed = true; });

    const auto process_error = tricky::handlers(tricky::handler(
        [&payload_handlers](auto) noexcept
        {
            tricky::process_payload(payload_handlers);
            return -2;
        }));

    const char* fileName = "myfile.txt";
    result<void> r{eFileError::kPermission, TRICKY_SOURCE_LOCATION,
                   tricky::c_str(fileName)};
    process_error(r);
    ASSERT_TRUE(isPayloadProcessed);
}

TEST(TrickySimpleTest, LoadToPayloadMultipleValuesViaLoadOfVoidResult)
{
    bool isPayloadProcessed{};
    const auto payload_handlers = std::make_tuple(
        [&isPayloadProcessed](const tricky::e_source_location&, tricky::c_str)
        { isPayloadProcessed = true; });

    const auto process_error = tricky::handlers(tricky::handler(
        [&payload_handlers](auto) noexcept
        {
            tricky::process_payload(payload_handlers);
            return -2;
        }));

    const char* fileName = "myfile.txt";
    result<void> r{eFileError::kPermission};
    r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
    process_error(r);
    ASSERT_TRUE(isPayloadProcessed);
}
