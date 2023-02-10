#include <gtest/gtest.h>
#include <tricky/tricky.h>
#include <user_literals/user_literals.h>

#include "test_common.h"

class TrickyTest : public ::testing::Test
{
   protected:
    template <typename T>
    static constexpr auto handle_any_error = tricky::handlers(tricky::handler(
        [](auto) noexcept
        {
            if constexpr (not std::is_same_v<T, void>)
            {
                return T{};
            }
        }));

    template <typename T>
    static decltype(auto) process_any_error(T&& aResult) noexcept
    {
        using ResultT = utils::remove_cvref_t<decltype(aResult)>;
        using value_type = typename ResultT::value_type;
        return handle_any_error<value_type>(std::forward<T>(aResult));
    }
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
    enum class e_handler_id
    {
        k_none = 0,
        k_category,
        k_value,
        k_any
    };
    template <typename R>
    std::enable_if_t<tricky::is_result_v<utils::remove_cvref_t<R>>,
                     typename utils::remove_cvref_t<R>::value_type>
    process_result(R&& aResult) noexcept
    {
        const auto payload_handlers = std::make_tuple(
            [this](char) { payload_flag_ = e_payload::k_char; },
            [this](float, char) { payload_flag_ = e_payload::k_float_char; },
            [this](float, char, uint32_t)
            { payload_flag_ = e_payload::k_float_char_uint32_t; },
            [this](const tricky::e_source_location&)
            { payload_flag_ = e_payload::k_src_location; });

        using value_type = typename utils::remove_cvref_t<R>::value_type;

        const auto process_error = tricky::handlers(
            tricky::handler<eFileError>(
                [&payload_handlers, this](auto) noexcept
                {
                    handler_id_ = e_handler_id::k_category;
                    tricky::process_payload(payload_handlers);
                    if constexpr (not std::is_same_v<value_type, void>)
                    {
                        return value_type{};
                    }
                }),
            tricky::handler<eReaderError::kError2, eFileError::kPermission>(
                [&payload_handlers, this](auto) noexcept
                {
                    handler_id_ = e_handler_id::k_value;
                    tricky::process_payload(payload_handlers);
                    if constexpr (not std::is_same_v<value_type, void>)
                    {
                        return value_type{};
                    }
                }),
            tricky::handler(
                [&payload_handlers, this](auto) noexcept
                {
                    handler_id_ = e_handler_id::k_any;
                    tricky::process_payload(payload_handlers);
                    if constexpr (not std::is_same_v<value_type, void>)
                    {
                        return value_type{};
                    }
                }));

        payload_flag_ = e_payload::k_none;
        handler_id_ = e_handler_id::k_none;
        if constexpr (not std::is_same_v<value_type, void>)
        {
            return process_error(std::move(aResult));
        }
        else
        {
            process_error(std::move(aResult));
        }
    }

    e_payload payload_flag_{};
    e_handler_id handler_id_{};
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
    process_any_error(std::move(r));
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
    process_any_error(std::move(r));
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
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, CallErrorForConstRValue)
{
    const result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(std::move(r).error<eBufferError>(),
              eBufferError::kInvalidPointer);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, CallErrorForRValue)
{
    result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(std::move(r).error<eBufferError>(),
              eBufferError::kInvalidPointer);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, CallErrorForConstRef)
{
    const result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, CallErrorForRef)
{
    result<void> r{eBufferError::kInvalidPointer};
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, ConversionFromBufferResult)
{
    buffer::result<void> bufResult(eBufferError::kInvalidPointer);
    result<void> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eBufferError>());
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, ConversionFromWriterResult)
{
    writer::result<int16_t> bufResult(eWriterError::kError4);
    result<int16_t> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, ConstConversion)
{
    const buffer::result<int> bufResult(eBufferError::kInvalidPointer);
    const result<int> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eBufferError>());
    ASSERT_EQ(r.error<eBufferError>(), eBufferError::kInvalidPointer);
    process_any_error(std::move(r));
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
    result<int16_t> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, Conversion2)
{
    writer::result<void> bufResult(eWriterError::kError4);
    result<int16_t> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, Conversion3)
{
    writer::result<int16_t> bufResult(eWriterError::kError4);
    result<void> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, Conversion4)
{
    writer::result<int16_t> bufResult(eWriterError::kError4);
    result<void> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, Conversion5)
{
    writer::result<void> bufResult(eWriterError::kError4);
    result<void> r(std::move(bufResult));
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.is_active_type<eWriterError>());
    ASSERT_EQ(r.error<eWriterError>(), eWriterError::kError4);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, Conversion6)
{
    writer::result<void> bufResult{};
    ASSERT_TRUE(bufResult.is_active_type<void>());
    result<void> r(std::move(bufResult));
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r.is_active_type<void>());
}

TEST_F(TrickyTest, Conversion7)
{
    result<void> bufResult{eFileError::kPermission};
    ASSERT_TRUE(bufResult.is_active_type<eFileError>());
    result<char> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eFileError>());
    ASSERT_EQ(r.error<eFileError>(), eFileError::kPermission);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, Conversion8)
{
    subset::result<void> bufResult{eFileError::kPermission};
    ASSERT_TRUE(bufResult.is_active_type<eFileError>());
    result<char> r(std::move(bufResult));
    ASSERT_TRUE(r.has_error());
    ASSERT_TRUE(r.is_active_type<eFileError>());
    ASSERT_EQ(r.error<eFileError>(), eFileError::kPermission);
    process_any_error(std::move(r));
}

TEST_F(TrickyTest, OneErrorValueHandlerWithoutArguments)
{
    eReaderError activeError{};
    const auto handle_result =
        tricky::handlers(tricky::handler<eReaderError::kError2>(
            [&activeError]() noexcept
            {
                activeError = eReaderError::kError2;
                return reader::result<int>{-1};
            }));

    {
        reader::result<int> r = eReaderError::kError2;
        handle_result(std::move(r));
        ASSERT_EQ(activeError, eReaderError::kError2);
    }

    {
        reader::result<int> r = eReaderError::kError1;
        handle_result(std::move(r));
        ASSERT_EQ(activeError, eReaderError::kError2);
        process_any_error(std::move(r));
    }
}

TEST_F(TrickyTest, OneErrorValueHandlerWithArguments)
{
    eReaderError activeError{};
    const auto handle_result =
        tricky::handlers(tricky::handler<eReaderError::kError2>(
            [&activeError](auto) noexcept
            {
                activeError = eReaderError::kError2;
                return reader::result<int>{-1};
            }));

    {
        reader::result<int> r = eReaderError::kError2;
        handle_result(std::move(r));
        ASSERT_EQ(activeError, eReaderError::kError2);
    }

    {
        reader::result<int> r = eReaderError::kError1;
        const auto r2 = handle_result(std::move(r));
        ASSERT_EQ(activeError, eReaderError::kError2);
        process_any_error(std::move(r2));
    }
}

TEST_F(TrickyTest, HandleTwoErrorValues)
{
    bool is_processed{false};
    const auto process_error = tricky::handlers(
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_processed](auto) noexcept
            {
                is_processed = true;
                return result<int>{-1};
            }));

    {
        result<int> r = eReaderError::kError2;
        process_error(std::move(r));
        ASSERT_TRUE(is_processed);
    }

    {
        is_processed = false;
        result<int> r = eFileError::kPermission;
        process_error(std::move(r));
        ASSERT_TRUE(is_processed);
    }
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
                return result<int>{-1};
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            {
                is_value_processed = true;
                return result<int>{-1};
            }));

    {
        result<int> r = eReaderError::kError2;
        process_error(std::move(r));
        ASSERT_TRUE(is_value_processed);
        ASSERT_EQ(file_error, eFileError::kOpenError);
    }

    {
        is_value_processed = false;
        result<int> r = eFileError::kPermission;
        process_error(std::move(r));
        ASSERT_TRUE(is_value_processed);
        ASSERT_EQ(file_error, eFileError::kOpenError);
    }

    {
        is_value_processed = false;
        result<int> r = eFileError::kAccessDenied;
        process_error(std::move(r));
        ASSERT_EQ(file_error, eFileError::kAccessDenied);
        ASSERT_FALSE(is_value_processed);
    }
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
                return 1;
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            {
                is_value_processed = true;
                return 2;
            }),
        tricky::handler(
            [&is_handler_for_any_error_called](auto) noexcept
            {
                is_handler_for_any_error_called = true;
                return 3;
            }));

    {
        result<int> r = eReaderError::kError2;
        static_assert(sizeof(result<int>) == sizeof(int),
                      "sizeof(result<T>) must be equal to sizeof(T)");
        process_error(std::move(r));
        ASSERT_TRUE(is_value_processed);
        ASSERT_EQ(file_error, eFileError::kOpenError);
        ASSERT_FALSE(is_handler_for_any_error_called);
    }

    {
        is_value_processed = false;
        result<int> r = eFileError::kPermission;
        process_error(std::move(r));
        ASSERT_TRUE(is_value_processed);
        ASSERT_EQ(file_error, eFileError::kOpenError);
        ASSERT_FALSE(is_handler_for_any_error_called);
    }

    {
        is_value_processed = false;
        result<int> r = eFileError::kAccessDenied;
        process_error(std::move(r));
        ASSERT_EQ(file_error, eFileError::kAccessDenied);
        ASSERT_FALSE(is_value_processed);
        ASSERT_FALSE(is_handler_for_any_error_called);
    }

    {
        file_error = eFileError::kOpenError;
        result<int> r = eBufferError::kInvalidPointer;
        process_error(std::move(r));
        ASSERT_EQ(file_error, eFileError::kOpenError);
        ASSERT_FALSE(is_value_processed);
        ASSERT_TRUE(is_handler_for_any_error_called);
    }
}

TEST_F(TrickyTest, VoidHandlersForTwoErrorValuesForOneCategoryAndForRest)
{
    bool is_value_processed{false};
    bool is_handler_for_any_error_called{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>([&file_error](auto aError) noexcept
                                    { file_error = aError; }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            { is_value_processed = true; }),
        tricky::handler([&is_handler_for_any_error_called](auto) noexcept
                        { is_handler_for_any_error_called = true; }));

    {
        result<void> r = eReaderError::kError2;
        process_error(std::move(r));
        ASSERT_TRUE(is_value_processed);
        ASSERT_EQ(file_error, eFileError::kOpenError);
        ASSERT_FALSE(is_handler_for_any_error_called);
    }

    {
        is_value_processed = false;
        result<void> r = eFileError::kPermission;
        process_error(std::move(r));
        ASSERT_TRUE(is_value_processed);
        ASSERT_EQ(file_error, eFileError::kOpenError);
        ASSERT_FALSE(is_handler_for_any_error_called);
    }

    {
        is_value_processed = false;
        result<void> r = eFileError::kAccessDenied;
        process_error(std::move(r));
        ASSERT_EQ(file_error, eFileError::kAccessDenied);
        ASSERT_FALSE(is_value_processed);
        ASSERT_FALSE(is_handler_for_any_error_called);
    }

    {
        file_error = eFileError::kOpenError;
        result<void> r = eBufferError::kInvalidPointer;
        process_error(std::move(r));
        ASSERT_EQ(file_error, eFileError::kOpenError);
        ASSERT_FALSE(is_value_processed);
        ASSERT_TRUE(is_handler_for_any_error_called);
    }
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
    process_any_error(std::move(r));
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload1)
{
    result<void> r = eReaderError::kError2;
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_value);
    ASSERT_EQ(payload_flag_, e_payload::k_none);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload2)
{
    result<void> r = eFileError::kPermission;
    r.load('j');
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_value);
    ASSERT_EQ(payload_flag_, e_payload::k_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload3)
{
    result<int> r = eFileError::kPermission;
    r.load(1.23f);
    r.load('j');
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_value);
    ASSERT_EQ(payload_flag_, e_payload::k_float_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload4)
{
    result<int> r = eFileError::kSystemError;
    r.load(1.23f);
    r.load('j');
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_category);
    ASSERT_EQ(payload_flag_, e_payload::k_float_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload5)
{
    network::result<int> r = eNetworkError::kLostConnection;
    r.load(1.23f);
    r.load('j');
    r.load(3456_u32);
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_any);
    ASSERT_EQ(payload_flag_, e_payload::k_float_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload6)
{
    network::result<int> r = eNetworkError::kUnreachableHost;
    r.load('j');
    r.load(3456_u32);
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_any);
    ASSERT_EQ(payload_flag_, e_payload::k_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload7)
{
    result<int> r = eFileError::kBusyDescriptor;
    r.load('j');
    r.load(3456_u32);
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_category);
    ASSERT_EQ(payload_flag_, e_payload::k_char);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload8)
{
    result<int> r = 10;
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_none);
    ASSERT_EQ(payload_flag_, e_payload::k_none);
}

TEST_F(TrickyHandlersTest, HandleErrorWithPayload9)
{
    result<int> r = TRICKY_NEW_ERROR(eFileError::kBusyDescriptor);
    process_result(std::move(r));
    ASSERT_EQ(handler_id_, e_handler_id::k_category);
    ASSERT_EQ(payload_flag_, e_payload::k_src_location);
}

TEST(TrickySimpleTest, HandleErrorForWhichHandlerDoesNotProvided)
{
    const auto process_error =
        tricky::handlers(tricky::handler<eFileError::kEOF>(
            [](auto) noexcept { return result<int>{-2}; }));
    result<int> r = TRICKY_NEW_ERROR(eFileError::kPermission);
    auto ret = process_error(std::move(r));
    ASSERT_TRUE(ret.has_error());

    const auto swallow_error =
        tricky::handlers(tricky::handler([](auto) noexcept { return -1; }));
    ASSERT_EQ(swallow_error(std::move(ret)), -1);
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
    process_error(std::move(r));
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
    process_error(std::move(r));
    ASSERT_TRUE(isPayloadProcessed);
}

TEST(TrickySimpleTest, LoadToPayloadMultipleValuesViaConstructorOfVoidResult)
{
    bool isPayloadProcessed{};
    const auto payload_handlers = std::make_tuple(
        [&isPayloadProcessed](const tricky::e_source_location&, tricky::c_str)
        { isPayloadProcessed = true; });

    const auto process_error = tricky::handlers(
        tricky::handler([&payload_handlers](auto) noexcept
                        { tricky::process_payload(payload_handlers); }));

    const char* fileName = "myfile.txt";
    result<void> r{eFileError::kPermission, TRICKY_SOURCE_LOCATION,
                   tricky::c_str(fileName)};
    process_error(std::move(r));
    ASSERT_TRUE(isPayloadProcessed);
}

TEST(TrickySimpleTest, LoadToPayloadMultipleValuesViaLoadOfVoidResult)
{
    bool isPayloadProcessed{};
    const auto payload_handlers = std::make_tuple(
        [&isPayloadProcessed](const tricky::e_source_location&, tricky::c_str)
        { isPayloadProcessed = true; });

    const auto process_error = tricky::handlers(
        tricky::handler([&payload_handlers](auto) noexcept
                        { tricky::process_payload(payload_handlers); }));

    const char* fileName = "myfile.txt";
    result<void> r{eFileError::kPermission};
    r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
    process_error(std::move(r));
    ASSERT_TRUE(isPayloadProcessed);
}

TEST(TrickySimpleTest, TryHandleAll1)
{
    bool is_value_processed{false};
    bool is_handler_for_any_error_called{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>([&file_error](auto aError) noexcept
                                    { file_error = aError; }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            { is_value_processed = true; }),
        tricky::handler([&is_handler_for_any_error_called](auto) noexcept
                        { is_handler_for_any_error_called = true; }));

    tricky::try_handle_all(
        []() noexcept
        {
            const char* fileName = "myfile.txt";
            result<void> r{eFileError::kPermission};
            r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
            return r;
        },
        std::move(process_error));

    ASSERT_TRUE(is_value_processed);
}

TEST(TrickySimpleTest, TryHandleAll2)
{
    bool is_value_processed{false};
    bool is_handler_for_any_error_called{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>(
            [&file_error](auto aError) noexcept
            {
                file_error = aError;
                return 1;
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            {
                is_value_processed = true;
                return 2;
            }),
        tricky::handler(
            [&is_handler_for_any_error_called](auto) noexcept
            {
                is_handler_for_any_error_called = true;
                return 3;
            }));

    const auto value = tricky::try_handle_all(
        []() noexcept
        {
            const char* fileName = "myfile.txt";
            result<int> r{eFileError::kPermission};
            r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
            return r;
        },
        std::move(process_error));

    static_assert(std::is_same_v<std::remove_const_t<decltype(value)>, int>);
    ASSERT_EQ(value, 2);
    ASSERT_TRUE(is_value_processed);
}

TEST(TrickySimpleTest, TryHandleSome1)
{
    bool is_value_processed{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>(
            [&file_error](auto aError) noexcept
            {
                file_error = aError;
                return result<void>{};
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            {
                is_value_processed = true;
                return result<void>{};
            }));

    auto res = tricky::try_handle_some(
        []() noexcept
        {
            const char* fileName = "myfile.txt";
            result<void> r{eFileError::kPermission};
            r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
            return r;
        },
        std::move(process_error));

    static_assert(std::is_same_v<typename decltype(res)::value_type, void>);
    ASSERT_TRUE(res);
    ASSERT_TRUE(is_value_processed);
}

TEST(TrickySimpleTest, TryHandleSome2)
{
    bool is_value_processed{false};
    eFileError file_error{eFileError::kOpenError};
    const auto process_error = tricky::handlers(
        tricky::handler<eFileError>(
            [&file_error](auto aError) noexcept
            {
                file_error = aError;
                return result<int>{3};
            }),
        tricky::handler<eReaderError::kError2, eFileError::kPermission>(
            [&is_value_processed](auto) noexcept
            {
                is_value_processed = true;
                return result<int>{};
            }));

    auto res = tricky::try_handle_some(
        []() noexcept
        {
            const char* fileName = "myfile.txt";
            result<int> r{eFileError::kFileNotFound};
            r.load(TRICKY_SOURCE_LOCATION, tricky::c_str(fileName));
            return r;
        },
        std::move(process_error));

    ASSERT_TRUE(res);
    ASSERT_EQ(res.value(), 3);
    ASSERT_EQ(file_error, eFileError::kFileNotFound);
}
