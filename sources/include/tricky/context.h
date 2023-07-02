#ifndef tricky_context_h
#define tricky_context_h

#include <strong_type/strong_type.h>
#include <utils/utils.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>

#include "error.h"

namespace tricky
{
template <typename Payload, typename... Errors>
class context;

template <typename T>
struct is_context : std::false_type
{
};

template <typename Payload, typename... Errors>
struct is_context<context<Payload, Errors...>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_context_v = is_context<T>::value;

namespace details
{
namespace ctx
{
template <typename T, typename E>
void set_error(E aError, T &aCtx) noexcept
{
    static_assert(is_context_v<T>);
    assert(!aCtx.has_error());
    static_assert(T::error_type_list::template contains_v<E>);

    new (aCtx.data_) error(aError);

    aCtx.set(T::kHasError);
}

template <typename T>
void reset_error(T &aCtx) noexcept
{
    assert(aCtx.has_error());
    reinterpret_cast<error *>(aCtx.data_)->~error();
    aCtx.reset(T::kHasError);
}
}  // namespace ctx
}  // namespace details

template <typename Payload, typename... Errors>
class context
{
    template <typename T, typename E>
    friend void details::ctx::set_error(E aError, T &aCtx) noexcept;

    friend void details::ctx::reset_error<context>(context &aCtx) noexcept;

   public:
    using error_type_list = utils::type_list<Errors...>;
    using payload_t = Payload;

   private:
    enum : std::uint8_t
    {
        kIsActive = 0b00000001,
        kHasError = 0b00000010
    };

    void move_error_helper(context &aCtx) noexcept
    {
        aCtx.payload_ = nullptr;
        if (has_error())
        {
            details::ctx::reset_error(*this);
        }

        if (aCtx.has_error())
        {
            new (data_) class error(std::move(aCtx.error()));
            this->set(kHasError);
            details::ctx::reset_error(aCtx);
        }
    }

   public:
    context() noexcept = default;

    context(const context &) = delete;
    context &operator=(const context &) = delete;

    context(context &&aCtx) noexcept : payload_{aCtx.payload_}
    {
        move_error_helper(aCtx);
    }

    context &operator=(context &&aCtx) noexcept
    {
        if (this != &aCtx)
        {
            payload_ = aCtx.payload_;
            move_error_helper(aCtx);
        }
        return *this;
    }

    constexpr context(Payload *aPayload) noexcept : payload_(aPayload)
    {
        static_assert(sizeof...(Errors) > 0);
    }

    class error &error() noexcept
    {
        assert(has_error());
        return *reinterpret_cast<class error *>(data_);
    }

    const class error &error() const noexcept
    {
        assert(has_error());
        return *reinterpret_cast<class error const *>(data_);
    }

    template <typename T>
    void load(T &&aArg) noexcept
    {
        assert(payload_ && "invalid payload_");
        payload_->load(std::forward<T>(aArg));
    }

    template <typename E>
    std::enable_if_t<error_type_list::template contains_v<E>, bool> has_error()
        const noexcept
    {
        return has_error() && error().template contains<E>();
    }

    inline bool has_error() const noexcept { return check(kHasError); }

    template <typename E>
    std::enable_if_t<error_type_list::template contains_v<E>, E> get_error()
        const noexcept
    {
        assert(has_error<E>());
        return *reinterpret_cast<E const *>(data_);
    }

    payload_t const *payload() const noexcept { return payload_; }
    payload_t *payload() noexcept { return payload_; }

    inline bool is_active() const noexcept { return check(kIsActive); }

    void activate() noexcept
    {
        assert(!is_active());
        set(kIsActive);
    }

    void deactivate() noexcept
    {
        assert(is_active());
        reset(kIsActive);
    }

    ~context()
    {
        static_assert(
            std::max({sizeof(Errors)...}) <= error::kMaxSize,
            "Some of Errors can take more size than provided by default. You "
            "can increase default size by setting compilation flag: "
            "TRICKY_MAX_ERROR_SIZE=128(or any value of your choice)");
        static_assert(
            std::max({alignof(Errors)...}) <= error::kMaxAlignment,
            "Some of Errors have more strict alignment than provided by "
            "default. You can increase default alignment by setting "
            "compilation flag: TRICKY_MAX_ERROR_ALIGNMENT=128(or any value of "
            "your choice that is power of 2)");
        assert(!has_error() &&
               "error value and its payload will be lost and never has a "
               "chance to be handled");
    }

   private:
    inline bool check(std::uint8_t aFlag) const noexcept
    {
        return state_ & aFlag;
    }

    inline void set(std::uint8_t aFlag) noexcept { state_ |= aFlag; }

    inline void reset(std::uint8_t aFlag) noexcept { state_ &= ~aFlag; }

    alignas(class error) std::byte data_[sizeof(class error)]{};
    payload_t *payload_{nullptr};
    std::uint8_t state_{};
};
}  // namespace tricky

#endif /* tricky_context_h */
