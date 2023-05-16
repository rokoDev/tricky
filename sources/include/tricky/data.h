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
}  // namespace tricky

#endif /* tricky_data_h */
