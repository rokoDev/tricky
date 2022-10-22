#include <gtest/gtest.h>
#include <tricky/payload.h>
#include <user_literals/user_literals.h>

#include <cstring>
#include <set>

template <typename T>
class Counter
{
    static std::size_t total_count_;
    static std::set<T const *> default_constructed_;
    static std::set<T const *> copy_constructed_;
    static std::set<T const *> move_constructed_;

    static inline void inc_total() noexcept { ++total_count_; }

    static inline void dec_total() noexcept { --total_count_; }

   public:
    Counter() noexcept
    {
        inc_total();
        default_constructed_.insert(static_cast<T const *>(this));
    }

    Counter(const Counter &) noexcept
    {
        inc_total();
        copy_constructed_.insert(static_cast<T const *>(this));
    }

    Counter(Counter &&) noexcept
    {
        inc_total();
        move_constructed_.insert(static_cast<T const *>(this));
    }

    ~Counter()
    {
        dec_total();
        default_constructed_.erase(static_cast<T const *>(this));
        copy_constructed_.erase(static_cast<T const *>(this));
        move_constructed_.erase(static_cast<T const *>(this));
    }

    Counter &operator=(const Counter &) noexcept = default;

    Counter &operator=(Counter &&) noexcept = default;

    static std::size_t total_count() noexcept { return total_count_; }

    static std::size_t default_count() noexcept
    {
        return default_constructed_.size();
    }

    static std::size_t copy_count() noexcept
    {
        return copy_constructed_.size();
    }

    static std::size_t move_count() noexcept
    {
        return move_constructed_.size();
    }
};

template <typename T>
std::size_t Counter<T>::total_count_{};
template <typename T>
std::set<T const *> Counter<T>::default_constructed_{};
template <typename T>
std::set<T const *> Counter<T>::copy_constructed_{};
template <typename T>
std::set<T const *> Counter<T>::move_constructed_{};

struct UserData : public Counter<UserData>
{
    uint8_t u8{};
    float f{};
    uint64_t u64{};
    char c{};

    UserData(uint8_t aU8, float aF, uint64_t aU64, char aC) noexcept
        : Counter<UserData>(), u8{aU8}, f{aF}, u64{aU64}, c{aC}
    {
    }

    friend bool operator==(const UserData &aLhs, const UserData &aRhs) noexcept
    {
        return (aLhs.u8 == aRhs.u8) &&
               !std::memcmp(&aLhs.f, &aRhs.f, sizeof(decltype(aLhs.f))) &&
               (aLhs.u64 == aRhs.u64) && (aLhs.c == aRhs.c);
    }
};

template <std::size_t MaxSpace, std::size_t MaxCount>
class Payload : public ::testing::Test
{
   protected:
    using payload = tricky::payload<MaxSpace, MaxCount>;
    using e_source_location = tricky::e_source_location;
    using c_str = tricky::c_str;
    template <typename T>
    using sequence = tricky::sequence<T>;
    payload data;
};

template <std::size_t MaxSpace, std::size_t MaxCount>
class LoadedPayload : public Payload<MaxSpace, MaxCount>
{
    using base = Payload<MaxSpace, MaxCount>;

   protected:
    using payload = typename base::payload;
    using e_source_location = typename base::e_source_location;
    using c_str = typename base::c_str;
    template <typename T>
    using sequence = typename base::template sequence<T>;
    void SetUp() override
    {
        const auto s = "some string";
        const auto origin = std::make_tuple(
            1.23f, '<', UserData{123_u8, 234.58f, 8489338094_u64, 'f'},
            c_str(s));
        base::data.load(std::get<0>(origin));
        base::data.load(std::get<1>(origin));
        base::data.load(std::get<2>(origin));
        base::data.load(std::get<3>(origin));
    }

    void TearDown() override
    {
        ASSERT_TRUE(
            (base::data.template match<float, char, UserData, c_str>()));
    }
};

using PayloadS128N10Test = Payload<128, 10>;
using PayloadSfloatN10Test = Payload<sizeof(float), 10>;
using LoadedPayloadTest = LoadedPayload<128, 10>;

TEST_F(PayloadS128N10Test, InitialValues)
{
    static_assert(payload::kMaxSpace == 128);
    static_assert(payload::kMaxCount == 10);
    ASSERT_EQ(data.max_space(), 128);
    ASSERT_EQ(data.max_count(), 10);
    ASSERT_EQ(data.count(), 0);
    ASSERT_EQ(data.space_used(), 0);
    ASSERT_EQ(data.space_shortage(), 0);
}

TEST_F(PayloadS128N10Test, OneValue)
{
    constexpr auto value = 12345.8f;
    data.load(value);

    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);

    ASSERT_TRUE(data.match<float>());

    const auto extracted = data.extract<float>(0);
    ASSERT_FLOAT_EQ(extracted, value);
}

TEST_F(PayloadS128N10Test, TwoValues)
{
    data.load(12345.8f);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);

    data.load(8000000008_u64);
    ASSERT_EQ(data.count(), 2);
    ASSERT_EQ(data.space_shortage(), 0);

    ASSERT_TRUE((data.match<float, uint64_t>()));

    const auto extracted_1 = data.extract<float>(0);
    ASSERT_FLOAT_EQ(extracted_1, 12345.8f);

    const auto extracted_2 = data.extract<uint64_t>(1);
    ASSERT_EQ(extracted_2, 8000000008_u64);
}

TEST_F(PayloadSfloatN10Test, NotEnoughSpaceForTwoValues)
{
    data.load(123_i8);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<int8_t>());

    data.load(8000000008_u64);
    ASSERT_EQ(data.count(), 1);
    ASSERT_NE(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<int8_t>());
    ASSERT_FALSE((data.match<int8_t, uint64_t>()));

    data.reset();
    ASSERT_EQ(data.count(), 0);
    ASSERT_EQ(data.space_used(), 0);
    ASSERT_EQ(data.space_shortage(), 0);
}

TEST_F(PayloadS128N10Test, ThreeValues)
{
    e_source_location location{__FILE__, __LINE__, __FUNCTION__};

    data.load(12345.8f);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);

    data.load(location);
    ASSERT_EQ(data.count(), 2);
    ASSERT_EQ(data.space_shortage(), 0);

    data.load(8000000008_u64);
    ASSERT_EQ(data.count(), 3);
    ASSERT_EQ(data.space_shortage(), 0);

    ASSERT_TRUE((data.match<float, e_source_location, uint64_t>()));

    const auto extracted_1 = data.extract<float>(0);
    ASSERT_FLOAT_EQ(extracted_1, 12345.8f);

    const auto extracted_2 = data.extract<e_source_location>(1);
    ASSERT_EQ(extracted_2, location);

    const auto extracted_3 = data.extract<uint64_t>(2);
    ASSERT_EQ(extracted_3, 8000000008_u64);
}

TEST_F(PayloadS128N10Test, NullptrCStr)
{
    char const *str = nullptr;
    const auto original = c_str(str);

    data.load(original);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_used(), 0);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<c_str>());

    const auto restored = data.extract<c_str>(0);
    ASSERT_EQ(restored, original);
}

TEST_F(PayloadS128N10Test, EmptyCStr)
{
    char const *str = "";
    const auto original = c_str(str);

    data.load(original);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_used(), 1);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<c_str>());

    const auto restored = data.extract<c_str>(0);
    ASSERT_EQ(restored, original);
}

TEST_F(PayloadS128N10Test, NonEmptyCStr)
{
    char const *str = "NonEmptyCStr";
    const auto original = c_str(str);

    data.load(original);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_used(), std::strlen(str) + 1);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<c_str>());

    const auto restored = data.extract<c_str>(0);
    ASSERT_EQ(restored, original);
}

TEST_F(PayloadS128N10Test, NonEmptyCStrOverflow)
{
    const auto original = c_str("NonEmptyCStr");
    const auto b_size = std::strlen(original.data()) + 1;
    const auto whole = payload::kMaxSpace / b_size;
    const auto rest = payload::kMaxSpace % b_size;
    const auto expected_shortage = b_size - rest;

    for (std::size_t i = 0; i < whole; ++i)
    {
        data.load(original);
        ASSERT_EQ(data.count(), i + 1);
        ASSERT_EQ(data.space_used(), b_size * (i + 1));
        ASSERT_EQ(data.space_shortage(), 0);
        const auto restored = data.extract<c_str>(i);
        ASSERT_EQ(restored, original);
    }
    ASSERT_TRUE((data.match<c_str, c_str, c_str, c_str>()));

    for (std::size_t i = 0; i < 10; ++i)
    {
        data.load(original);
        ASSERT_EQ(data.count(), whole);
        ASSERT_EQ(data.space_used(), b_size * whole);
        ASSERT_EQ(data.space_shortage(), expected_shortage);
    }

    data.reset();
    ASSERT_EQ(data.count(), 0);
    ASSERT_EQ(data.space_used(), 0);
}

TEST_F(PayloadS128N10Test, SaveUserData)
{
    const UserData original{121_u8, 234.56f, 8489338092_u64, 'h'};
    data.load(original);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<UserData>());
    const auto &restored = data.extract<UserData>(0);
    ASSERT_EQ(restored, original);
    ASSERT_EQ(UserData::total_count(), 2);
    ASSERT_EQ(UserData::copy_count(), 1);
    data.reset();
    ASSERT_EQ(UserData::total_count(), 1);
}

TEST_F(PayloadS128N10Test, SaveNullUserDataSequence)
{
    const auto original = sequence<UserData>(nullptr, 0);

    data.load(original);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<sequence<UserData>>());
    ASSERT_EQ(UserData::total_count(), 0);

    const auto &restored = data.extract<sequence<UserData>>(0);
    ASSERT_EQ(restored, original);
    ASSERT_EQ(UserData::total_count(), 0);

    data.reset();
    ASSERT_EQ(UserData::total_count(), 0);
    ASSERT_EQ(data.count(), 0);
}

TEST_F(PayloadS128N10Test, SaveUserDataSequence)
{
    const auto seq =
        utils::make_array(UserData{121_u8, 234.56f, 8489338092_u64, 'h'},
                          UserData{122_u8, 234.57f, 8489338093_u64, 'g'},
                          UserData{123_u8, 234.58f, 8489338094_u64, 'f'},
                          UserData{124_u8, 234.59f, 8489338095_u64, 'k'});
    ASSERT_EQ(UserData::total_count(), seq.size());
    ASSERT_EQ(UserData::move_count(), seq.size());
    ASSERT_EQ(UserData::copy_count(), 0);
    const auto original = sequence<UserData>(seq.data(), seq.size());

    data.load(original);
    ASSERT_EQ(data.count(), 1);
    ASSERT_EQ(data.space_shortage(), 0);
    ASSERT_TRUE(data.match<sequence<UserData>>());
    ASSERT_EQ(UserData::total_count(), seq.size() * 2);
    ASSERT_EQ(UserData::move_count(), seq.size());
    ASSERT_EQ(UserData::copy_count(), seq.size());

    const auto &restored = data.extract<sequence<UserData>>(0);
    ASSERT_EQ(restored, original);
    ASSERT_EQ(UserData::total_count(), seq.size() * 2);

    data.reset();
    ASSERT_EQ(UserData::total_count(), seq.size());
    ASSERT_EQ(UserData::move_count(), seq.size());
    ASSERT_EQ(UserData::copy_count(), 0);
}

TEST_F(PayloadS128N10Test, SaveUserDataSequenceWithOverflow)
{
    const auto seq =
        utils::make_array(UserData{121_u8, 234.56f, 8489338092_u64, 'h'},
                          UserData{122_u8, 234.57f, 8489338093_u64, 'g'},
                          UserData{123_u8, 234.58f, 8489338094_u64, 'f'},
                          UserData{124_u8, 234.59f, 8489338095_u64, 'k'},
                          UserData{121_u8, 234.56f, 8489338092_u64, 'h'},
                          UserData{122_u8, 234.57f, 8489338093_u64, 'g'},
                          UserData{123_u8, 234.58f, 8489338094_u64, 'f'},
                          UserData{124_u8, 234.59f, 8489338095_u64, 'k'});

    const auto original = sequence<UserData>(seq.data(), seq.size());

    data.load(original);
    ASSERT_EQ(data.count(), 0);
    ASSERT_EQ(data.space_used(), 0);
    ASSERT_NE(data.space_shortage(), 0);
    ASSERT_FALSE(data.match<sequence<UserData>>());
    ASSERT_EQ(UserData::total_count(), seq.size());
}

TEST_F(LoadedPayloadTest, Process1)
{
    ASSERT_FALSE(data.process());
    ASSERT_TRUE(data.process([]() {}));
    ASSERT_FALSE(data.process([](char) {}));
}

TEST_F(LoadedPayloadTest, Process2)
{
    std::size_t processed_index{};
    ASSERT_TRUE(data.process([&processed_index](char) { processed_index = 1; },
                             [&processed_index]() { processed_index = 2; }));
    ASSERT_EQ(processed_index, 2);
}

TEST_F(LoadedPayloadTest, Process3)
{
    std::size_t processed_index{};
    ASSERT_TRUE(data.process([&processed_index](char) { processed_index = 1; },
                             [&processed_index](float)
                             { processed_index = 2; }));
    ASSERT_EQ(processed_index, 2);
}

TEST_F(LoadedPayloadTest, Process4)
{
    std::size_t processed_index{};
    ASSERT_TRUE(data.process([&processed_index](char) { processed_index = 1; },
                             [&processed_index](float, char, UserData, c_str)
                             { processed_index = 2; }));
    ASSERT_EQ(processed_index, 2);
}

TEST_F(LoadedPayloadTest, Process5)
{
    std::size_t processed_index{};
    ASSERT_TRUE(data.process([&processed_index](char) { processed_index = 1; },
                             [&processed_index](float, char)
                             { processed_index = 2; },
                             [&processed_index](float, char, UserData, c_str)
                             { processed_index = 3; }));
    ASSERT_EQ(processed_index, 2);
}

TEST_F(LoadedPayloadTest, Process6)
{
    std::size_t processed_index{};
    ASSERT_TRUE(data.process([&processed_index](char) { processed_index = 1; },
                             [&processed_index](float, UserData, char)
                             { processed_index = 2; },
                             [&processed_index](float, char, UserData, c_str)
                             { processed_index = 3; }));
    ASSERT_EQ(processed_index, 3);
}

TEST_F(LoadedPayloadTest, LoadMaxCountValues)
{
    for (std::size_t i = data.count(); i < data.max_count(); ++i)
    {
        data.load('p');
    }
    ASSERT_EQ(data.count(), data.max_count());

    std::size_t processed_index{};
    ASSERT_TRUE(data.process(
        [&processed_index](char) { processed_index = 1; },
        [&processed_index](float, UserData, char) { processed_index = 2; },
        [&processed_index](float, char, UserData, c_str, char, char, char, char,
                           char, char) { processed_index = 3; }));
    ASSERT_EQ(processed_index, 3);
}

TEST_F(LoadedPayloadTest, LoadMoreThanMaxCountValues)
{
    for (std::size_t i = data.count(); i < data.max_count() + 10; ++i)
    {
        data.load('p');
    }
    ASSERT_EQ(data.count(), data.max_count());

    std::size_t processed_index{};
    ASSERT_TRUE(data.process(
        [&processed_index](char) { processed_index = 1; },
        [&processed_index](float, UserData, char) { processed_index = 2; },
        [&processed_index](float, char, UserData, c_str, char, char, char, char,
                           char, char) { processed_index = 3; }));
    ASSERT_EQ(processed_index, 3);
}

TEST_F(LoadedPayloadTest, ProcessTuple)
{
    std::size_t processed_index{};
    const auto handlers = std::make_tuple(
        [&processed_index](char) { processed_index = 1; },
        [&processed_index](float, char) { processed_index = 2; },
        [&processed_index](float, char, UserData, c_str)
        { processed_index = 3; });
    ASSERT_TRUE(data.process(handlers));
    ASSERT_EQ(processed_index, 2);
}

namespace payload_test
{
void func(float, char, UserData, tricky::c_str) noexcept {}

void g(float, char, UserData) noexcept {}

}  // namespace payload_test

TEST_F(LoadedPayloadTest, ProcessPointerToFunction)
{
    using FuncP = void (*)(float, char, UserData, tricky::c_str) noexcept;
    FuncP f = payload_test::func;
    ASSERT_TRUE(data.process(f));
}

TEST_F(LoadedPayloadTest, ProcessWithFunction)
{
    ASSERT_TRUE(data.process(payload_test::g));
}

TEST_F(LoadedPayloadTest, ProcessWithFunctionOrPointerToFunction)
{
    using FuncP = void (*)(float, char, UserData, tricky::c_str) noexcept;
    FuncP f = payload_test::func;
    ASSERT_TRUE(data.process(payload_test::g, f));
}

TEST_F(LoadedPayloadTest, ProcessMixed)
{
    using FuncP = void (*)(float, char, UserData, tricky::c_str) noexcept;
    FuncP f = payload_test::func;
    ASSERT_TRUE(data.process(payload_test::g, f, []() {}));
}

TEST_F(LoadedPayloadTest, ProcessMixedTuple)
{
    using FuncP = void (*)(float, char, UserData, tricky::c_str) noexcept;
    FuncP f = payload_test::func;
    auto handlers = std::make_tuple([]() {}, payload_test::g, f);
    ASSERT_TRUE(data.process(handlers));
}
