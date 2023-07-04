#ifndef tricky_test_common_h
#define tricky_test_common_h

#include <tricky/tricky.h>

#include <ostream>
#include <sstream>
#include <tuple>

namespace test_utils
{
enum class eBigError : std::uint64_t
{
    kOne,
    kTwo,
    kThree
};

enum class eReaderError : std::uint8_t
{
    kError1,
    kError2,
};

enum class eWriterError : std::uint8_t
{
    kError3,
    kError4,
    kError5,
};

enum class eBufferError : std::uint8_t
{
    kInvalidIndex,
    kInvalidPointer,
};

enum class eFileError : std::uint8_t
{
    kOpenError,
    kEOF,
    kAccessDenied,
    kPermission,
    kBusyDescriptor,
    kFileNotFound,
    kSystemError
};

enum class eNetworkError : std::uint8_t
{
    kUnreachableHost,
    kLostConnection
};

template <typename T>
using result =
    tricky::result<T, eReaderError, eWriterError, eBufferError, eFileError>;

namespace buffer
{
template <typename T>
using result = tricky::result<T, eBufferError>;
}

namespace writer
{
template <typename T>
using result = tricky::result<T, eWriterError>;
}

namespace reader
{
template <typename T>
using result = tricky::result<T, eReaderError>;
}

namespace network
{
template <typename T>
using result = tricky::result<T, eNetworkError>;
}

namespace subset
{
template <typename T>
using result = tricky::result<T, eFileError, eBufferError, eWriterError>;
}

class center;
class right;
class left;

namespace details
{
template <typename T>
std::tuple<std::size_t, std::size_t> spaces(std::uint8_t aFieldWidth,
                                            std::size_t aValueLen);

template <>
[[maybe_unused]] std::tuple<std::size_t, std::size_t> spaces<center>(
    std::uint8_t aFieldWidth, std::size_t aValueLen)
{
    assert(aValueLen <= aFieldWidth);

    const auto left_spaces = (aFieldWidth - aValueLen) / 2;
    const auto right_spaces = aFieldWidth - aValueLen - left_spaces;
    return {left_spaces, right_spaces};
}

template <>
[[maybe_unused]] std::tuple<std::size_t, std::size_t> spaces<left>(
    std::uint8_t aFieldWidth, std::size_t aValueLen)
{
    assert(aValueLen <= aFieldWidth);
    return {0, aFieldWidth - aValueLen};
}

template <>
[[maybe_unused]] std::tuple<std::size_t, std::size_t> spaces<right>(
    std::uint8_t aFieldWidth, std::size_t aValueLen)
{
    assert(aValueLen <= aFieldWidth);
    return {aFieldWidth - aValueLen, 0};
}

struct FillerImpl
{
    static inline char const* value = u8" ";
    static void reset() noexcept { value = u8" "; }
};

struct WidthImpl
{
    static inline std::uint8_t value = 0;
    static void reset() noexcept { value = 0; }
};

template <typename T>
class base_alignment
{
    using Impl = T;

   public:
    base_alignment() = default;

    template <typename... V>
    base_alignment(V&&... aValues)
    {
        to_str_ = [this, &aValues...]()
        {
            const auto value_len = lenght(std::forward<V>(aValues)...);
            if (!width_)
            {
                width_ = static_cast<std::uint8_t>(value_len);
            }
            const auto [left_spaces, right_spaces] =
                spaces<T>(width_, value_len);
            print_to_ss(left_spaces, right_spaces, std::forward<V>(aValues)...);
        };
    }

    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& aOut,
                                                     Impl& aImpl)
    {
        aImpl.to_str_();
        aOut << aImpl.ss_.str();
        return aOut;
    }

    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& aOut,
                                                     Impl&& aImpl)
    {
        return aOut << aImpl;
    }

    decltype(auto) width(std::uint8_t aWidth) noexcept
    {
        width_ = aWidth;
        return impl();
    }

    decltype(auto) filler(char const* aFiller) noexcept
    {
        filler_ = aFiller;
        return impl();
    }

   protected:
    Impl& impl() noexcept { return static_cast<Impl&>(*this); }

    template <typename... V>
    std::size_t lenght(V&&... aValues)
    {
        std::stringstream value_ss;
        (value_ss << ... << aValues);
        const auto len = value_ss.tellp();
        assert(len >= 0);
        const auto value_len = static_cast<std::size_t>(len);
        return value_len;
    }

    void add_spaces(std::size_t aCount)
    {
        for (std::size_t i = 0; i < aCount; ++i)
        {
            ss_ << filler_;
        }
    }

    template <typename... V>
    void print_to_ss(std::size_t aLeft, std::size_t aRight, V&&... aValues)
    {
        add_spaces(aLeft);
        (void)(ss_ << ... << aValues);
        add_spaces(aRight);
    }

   private:
    std::stringstream ss_;
    std::function<void()> to_str_ = [this]() { print_to_ss(width_, 0); };
    std::uint8_t width_ = WidthImpl::value;
    char const* filler_ = FillerImpl::value;
};
}  // namespace details

[[maybe_unused]] void filler(char const* aValue) noexcept
{
    details::FillerImpl::value = aValue;
}

[[maybe_unused]] char const* filler() noexcept
{
    return details::FillerImpl::value;
}

[[maybe_unused]] void reset_filler() noexcept { details::FillerImpl::reset(); }

[[maybe_unused]] void field_width(std::uint8_t aValue) noexcept
{
    details::WidthImpl::value = aValue;
}

[[maybe_unused]] std::uint8_t field_width() noexcept
{
    return details::WidthImpl::value;
}

[[maybe_unused]] void reset_field_width() noexcept
{
    details::WidthImpl::reset();
}

class center : public details::base_alignment<center>
{
   public:
    template <typename... T>
    center(T&&... aValues) : base_alignment<center>(std::forward<T>(aValues)...)
    {
    }

    template <typename... T>
    center(std::uint8_t aSideExt, T&&... aValues)
    {
        print_to_ss(aSideExt, aSideExt, std::forward<T>(aValues)...);
    }
};

class left : public details::base_alignment<left>
{
    friend class base_alignment<left>;

   public:
    template <typename... T>
    left(T&&... aValues) : base_alignment<left>(std::forward<T>(aValues)...)
    {
    }
};

class right : public details::base_alignment<right>
{
    friend class base_alignment<right>;

   public:
    template <typename... T>
    right(T&&... aValues) : base_alignment<right>(std::forward<T>(aValues)...)
    {
    }
};

class table_row
{
   public:
    template <typename... T>
    table_row(T&&... aArgs)
    {
        to_str_ = [this, &aArgs...]()
        {
            std::invoke(apply_mode_, this);
            set_fillers(aArgs...);
            row(ss_, aArgs...);
        };
    }

    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& aOut,
                                                     table_row& aRow)
    {
        aRow.to_str_();
        aOut << aRow.ss_.str();
        return aOut;
    }

    [[maybe_unused]] friend std::ostream& operator<<(std::ostream& aOut,
                                                     table_row&& aRow)
    {
        return aOut << aRow;
    }

    table_row& top() noexcept
    {
        row_mode_ = eRowMode::kTop;
        apply_mode_ = &table_row::set_top;
        return *this;
    }

    table_row& middle() noexcept
    {
        row_mode_ = eRowMode::kMiddle;
        apply_mode_ = &table_row::set_middle;
        return *this;
    }

    table_row& bottom() noexcept
    {
        row_mode_ = eRowMode::kBottom;
        apply_mode_ = &table_row::set_bottom;
        return *this;
    }

    table_row& data_with_default_filler() noexcept
    {
        row_mode_ = eRowMode::kUserData;
        apply_mode_ = &table_row::set_user_data;
        is_custom_filler_ = false;
        return *this;
    }

    table_row& data_with_custom_fillers() noexcept
    {
        row_mode_ = eRowMode::kUserData;
        apply_mode_ = &table_row::set_user_data;
        is_custom_filler_ = true;
        return *this;
    }

   private:
    enum class eRowMode : std::uint8_t
    {
        kTop,
        kMiddle,
        kBottom,
        kUserData
    };

    void set_top() noexcept
    {
        assert(row_mode_ == eRowMode::kTop);
        left_ = top_left_;
        right_ = top_right_;
        delim_ = top_delim_;
        filler_ = top_filler_;
    }

    void set_middle() noexcept
    {
        assert(row_mode_ == eRowMode::kMiddle);
        left_ = middle_left_;
        right_ = middle_right_;
        delim_ = middle_delim_;
        filler_ = middle_filler_;
    }

    void set_bottom() noexcept
    {
        assert(row_mode_ == eRowMode::kBottom);
        left_ = bottom_left_;
        right_ = bottom_right_;
        delim_ = bottom_delim_;
        filler_ = bottom_filler_;
    }

    void set_user_data() noexcept
    {
        assert(row_mode_ == eRowMode::kUserData);
        left_ = user_data_left_;
        right_ = user_data_right_;
        delim_ = user_data_delim_;
        filler_ = user_data_filler_;
    }

    template <typename T>
    void row_tail(std::stringstream& aSS, T&& aArg)
    {
        aSS << delim_;
        aSS << aArg;
    }

    template <typename Head, typename... Tail>
    void row(std::stringstream& aSS, Head&& aHead, Tail&&... aTail)
    {
        aSS << left_;
        aSS << aHead;
        (..., row_tail(aSS, std::forward<Tail>(aTail)));
        aSS << right_ << '\n';
    }

    template <typename... T>
    void set_fillers(T&&... aArgs) noexcept
    {
        if (!((eRowMode::kUserData == row_mode_) && is_custom_filler_))
        {
            (..., aArgs.filler(filler_));
        }
    }

    using mode_setter = void (table_row::*)() noexcept;
    std::stringstream ss_;
    std::function<void()> to_str_ = []() {};

    mode_setter apply_mode_ = &table_row::set_user_data;
    eRowMode row_mode_ = eRowMode::kUserData;
    bool is_custom_filler_{};

    char const* left_ = 0;
    char const* right_ = 0;
    char const* delim_ = 0;
    char const* filler_ = 0;

    char const* top_left_ = u8"┏";
    char const* top_right_ = u8"┓";
    char const* top_delim_ = u8"┳";
    char const* top_filler_ = u8"━";

    char const* middle_left_ = u8"┣";
    char const* middle_right_ = u8"┫";
    char const* middle_delim_ = u8"╋";
    char const* middle_filler_ = u8"━";

    char const* bottom_left_ = u8"┗";
    char const* bottom_right_ = u8"┛";
    char const* bottom_delim_ = u8"┻";
    char const* bottom_filler_ = u8"━";

    char const* user_data_left_ = u8"┃";
    char const* user_data_right_ = u8"┃";
    char const* user_data_delim_ = u8"┃";
    char const* user_data_filler_ = u8" ";  // u8"∙";
};

namespace example
{
[[maybe_unused]] void aligned_print_test()
{
    constexpr std::size_t block_count = 32;
    const std::uint8_t kIndexWidth = 7;
    const std::uint8_t kWidth = 13;
    filler(u8"-");
    field_width(kWidth);

    std::cout << table_row(center().width(kIndexWidth), left(), right(),
                           center())
                     .top();
    std::cout << table_row(center("index").width(kIndexWidth), center("left"),
                           center("right"), center("total bits"));
    std::cout << table_row(center().width(kIndexWidth), left(), right(),
                           center())
                     .middle();

    filler(u8" ");
    std::stringstream left_bits_ss;
    std::stringstream right_bits_ss;
    for (std::size_t i = 0; i < block_count; ++i)
    {
        const std::size_t max_left_index = i;
        const std::size_t l_num_bits = utils::bits_count(max_left_index);
        left_bits_ss << std::bitset<CHAR_BIT>{max_left_index};

        const std::size_t max_right_index = block_count - 1 - i;
        const std::size_t r_num_bits = utils::bits_count(max_right_index);
        right_bits_ss << std::bitset<CHAR_BIT>{max_right_index};

        std::cout << table_row(
                         center(i).width(kIndexWidth),
                         center(max_left_index, ": ", left_bits_ss.str()),
                         center(max_right_index, ": ", right_bits_ss.str()),
                         center(l_num_bits + r_num_bits))
                         .data_with_custom_fillers();

        left_bits_ss.str(std::string());
        right_bits_ss.str(std::string());

        if (i < block_count - 1)
        {
            std::cout << table_row(center().width(kIndexWidth), left(), right(),
                                   center())
                             .middle();
        }
    }
    std::cout << table_row(center().width(kIndexWidth), left(), right(),
                           center())
                     .bottom();
}
}  // namespace example
}  // namespace test_utils

#endif /* tricky_test_common_h */
