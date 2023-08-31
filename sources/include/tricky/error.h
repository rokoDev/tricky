#ifndef tricky_error_h
#define tricky_error_h

#include <strong_type/strong_type.h>
#include <type_name/type_name.h>
#include <utils/utils.h>

#include <cassert>
#include <functional>
#include <new>
#include <type_traits>

namespace tricky
{
namespace details
{
namespace error_ops
{
template <typename StrongT>
struct DataPtrOps
    : strong::indirection<StrongT>
    , strong::subscription<StrongT>
    , strong::comparisons<StrongT>
    , strong::implicitly_convertible_to_underlying<StrongT>
{
};

using Src = strong::strong_type<struct SrcTag, std::byte const *, DataPtrOps>;
using Dst = strong::strong_type<struct DstTag, std::byte *, DataPtrOps>;

template <typename T>
using error_destroyer_t = void (*)(T &) noexcept;
using error_copier_t = void (*)(Dst aDst, Src aSrc) noexcept;

template <typename E, typename T>
void destroy_error(T &aErrorObject) noexcept
{
    aErrorObject.template reset_error<E>();
}

template <typename E>
void copy_error(Dst aDst, Src aSrc) noexcept
{
    static_assert(std::is_nothrow_copy_constructible_v<E>);
    assert(utils::is_aligned<E>(aDst) &&
           "aDst is not aligned to contain value of type E");
    assert(utils::is_aligned<E>(aSrc) &&
           "aSrc is not aligned to contain value of type E");
    new (aDst) E(*reinterpret_cast<E const *>(aSrc.get()));
}
}  // namespace error_ops
}  // namespace details

template <std::size_t MaxSize = sizeof(std::uint64_t),
          std::size_t MaxAlignment = alignof(std::uint64_t)>
class error
{
    template <typename E, typename T>
    friend void details::error_ops::destroy_error(T &) noexcept;

   public:
    static constexpr std::size_t kMaxSize = MaxSize;
    static constexpr std::size_t kMaxAlignment = MaxAlignment;

    ~error()
    {
        static_assert(utils::is_power_of_2(kMaxAlignment));
        if (error_destroyer_)
        {
            reset();
        }
    }

    error() = delete;
    error(const error &aOther) = delete;
    error &operator=(const error &aOther) = delete;

    error(error &&aOther) noexcept
        : data_{}
        , type_name_(std::cref(aOther.type_name_))
        , error_destroyer_{aOther.error_destroyer_}
        , error_copier_{aOther.error_copier_}
    {
        assert(is_valid());
        std::invoke(error_copier_, Dst(data_), Src(aOther.data_));
        aOther.reset();
    }

    error &operator=(error &&aOther) noexcept
    {
        assert(is_valid());
        if (this != &aOther)
        {
            assert(aOther.is_valid());
            reset();

            error_destroyer_ = aOther.error_destroyer_;
            error_copier_ = aOther.error_copier_;
            type_name_ = std::cref(aOther.type_name_);
            std::invoke(error_copier_, Dst(data_), Src(aOther.data_));
            aOther.reset();
        }
        return *this;
    }

    template <typename E, typename error_t = utils::remove_cvref_t<E>>
    error(E &&aError) noexcept
        : data_()
        , type_name_(std::cref(type_name::kName<error_t>))
        , error_destroyer_(details::error_ops::destroy_error<error_t, error>)
        , error_copier_(details::error_ops::copy_error<error_t>)
    {
        validate<error_t>();
        static_assert(std::is_nothrow_copy_constructible_v<error_t>);
        static_assert(std::is_nothrow_destructible_v<error_t>);
        assert(is_valid());
        new (data_) error_t(aError);
    }

    template <typename E>
    inline bool contains() const noexcept
    {
        validate<E>();
        return &type_name_.get() == &type_name::kName<E>;
    }

    inline const std::string_view &type_name() const noexcept
    {
        return type_name_.get();
    }

    template <typename E>
    E &value() noexcept
    {
        assert(contains<E>());
        return *reinterpret_cast<E *>(data_);
    }

    template <typename E>
    const E &value() const noexcept
    {
        assert(contains<E>());
        return *reinterpret_cast<E const *>(data_);
    }

    template <typename E>
    E value() &&noexcept
    {
        assert(contains<E>());
        return *reinterpret_cast<E *>(data_);
    }

    template <typename E>
    E value() const &&noexcept
    {
        assert(contains<E>());
        return *reinterpret_cast<E const *>(data_);
    }

    operator bool() const noexcept { return is_valid(); }

   private:
    using error_destroyer_t = details::error_ops::error_destroyer_t<error>;
    using error_copier_t = details::error_ops::error_copier_t;
    using Src = details::error_ops::Src;
    using Dst = details::error_ops::Dst;

    template <typename T>
    static void validate() noexcept
    {
        static_assert(
            sizeof(T) <= kMaxSize,
            "Error: Not enough memory in data_ for placement value of type T");
        static_assert(
            alignof(T) <= kMaxAlignment,
            "Error: data_ is not proper aligned for placement value of type T");
    }

    bool is_valid() const noexcept
    {
        return error_destroyer_ && error_copier_ &&
               (&type_name_.get() != &empty_);
    }

    template <typename E>
    void reset_error() noexcept
    {
        assert(contains<E>());
        assert(error_destroyer_);
        assert(error_copier_);
        reinterpret_cast<E *>(data_)->~E();
        error_destroyer_ = nullptr;
        error_copier_ = nullptr;
        type_name_ = std::cref(empty_);
    }

    void reset() noexcept
    {
        assert(error_destroyer_);
        std::invoke(error_destroyer_, *this);
    }

    static constexpr std::string_view empty_;

    alignas(kMaxAlignment) std::byte data_[kMaxSize]{};
    std::reference_wrapper<const std::string_view> type_name_;
    error_destroyer_t error_destroyer_{nullptr};
    error_copier_t error_copier_{nullptr};
};
}  // namespace tricky

#endif /* tricky_error_h */
