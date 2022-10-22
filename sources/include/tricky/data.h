#ifndef tricky_data_h
#define tricky_data_h

namespace tricky
{
class e_source_location
{
   public:
    e_source_location() = delete;
    constexpr e_source_location(char const *aFile, int aLine,
                                char const *aFunction) noexcept
        : file_(aFile), line_(aLine), function_(aFunction)
    {
        assert(file_ && *file_);
        assert(line_ > 0);
        assert(function_ && *function_);
    }

    constexpr char const *file() const noexcept { return file_; }

    constexpr int line() const noexcept { return line_; }

    constexpr char const *function() const noexcept { return function_; }

    friend constexpr bool operator==(const e_source_location &aLhs,
                                     const e_source_location &aRhs) noexcept
    {
        return (aLhs.line_ == aRhs.line_) && (aLhs.file_ == aRhs.file_) &&
               (aLhs.function_ == aRhs.function_);
    }

   private:
    char const *file_{nullptr};
    int line_{};
    char const *function_{nullptr};
};

template <typename T>
class sequence final
{
   public:
    using type = T;
    using count_type = std::size_t;

    friend bool operator==(const sequence &aLhs, const sequence &aRhs) noexcept
    {
        if (aLhs.count_ != aRhs.count_)
        {
            return false;
        }
        const auto is_equal = [aLhs, aRhs]() -> bool
        {
            std::size_t i = 0;
            while ((i < aLhs.count_) &&
                   (*(aLhs.data_ + i) == *(aRhs.data_ + i)))
                ++i;
            return i == aLhs.count_;
        };
        return (aLhs.data_ == aRhs.data_) || is_equal();
    }

    friend bool operator!=(const sequence &aLhs, const sequence &aRhs) noexcept
    {
        return !(aLhs == aRhs);
    }

    constexpr sequence(type const *aData, count_type aCount) noexcept
        : count_{aCount}, data_{aData}
    {
        assert((((aData != nullptr) && (aCount > 0)) ||
                ((aData == nullptr) && (aCount == 0))) &&
               "invalid data");
    }

    inline constexpr count_type count() const noexcept { return count_; }

    inline constexpr std::size_t size_of_data() const noexcept
    {
        return count_ * sizeof(type);
    }

    inline constexpr type const *data() const noexcept { return data_; }

   private:
    count_type count_{};
    type const *data_{};
};

template <typename T>
struct is_sequence : std::false_type
{
};

template <typename T>
struct is_sequence<sequence<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_sequence_v = is_sequence<T>::value;

class c_str final
{
   public:
    using type = char;
    using size_type = std::size_t;

    friend bool operator==(const c_str &aLhs, const c_str &aRhs) noexcept
    {
        return (aLhs.data_ == aRhs.data_) ||
               (aLhs.data_ && aRhs.data_ &&
                !std::strcmp(aLhs.data_, aRhs.data_));
    }

    friend bool operator!=(const c_str &aLhs, const c_str &aRhs) noexcept
    {
        return !(aLhs == aRhs);
    }

    c_str(type const *aData) noexcept : data_{aData} {}

    inline constexpr type const *data() const noexcept { return data_; }

   private:
    type const *data_{};
};
}  // namespace tricky

#endif /* tricky_data_h */
