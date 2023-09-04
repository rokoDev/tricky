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
template <typename Error, typename... RestErrors>
class context;

template <typename Payload, typename... Errors>
class heavy_context;

template <typename T>
struct is_context : std::false_type
{
};

template <typename... Errors>
struct is_context<context<Errors...>> : std::true_type
{
};

template <typename Payload, typename... Errors>
struct is_context<heavy_context<Payload, Errors...>> : std::true_type
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
    using error_t = typename T::error_t;

    new (aCtx.data_) error_t(aError);

    aCtx.set(T::kHasError);
}

template <typename T>
void reset_error(T &aCtx) noexcept
{
    assert(aCtx.has_error());
    using error_t = typename T::error_t;
    reinterpret_cast<error_t *>(aCtx.data_)->~error_t();
    aCtx.reset(T::kHasError);
}
}  // namespace ctx
}  // namespace details

class polymorphic_context
{
   public:
    virtual bool is_active() const noexcept = 0;
    virtual void activate() noexcept = 0;
    virtual void deactivate() noexcept = 0;

   protected:
    polymorphic_context() noexcept = default;
    virtual ~polymorphic_context() noexcept = default;

    thread_local static polymorphic_context *active_context_;
};

thread_local polymorphic_context *polymorphic_context::active_context_ =
    nullptr;

template <typename Error, typename... RestErrors>
class context
{
    template <typename T, typename E>
    friend void details::ctx::set_error(E aError, T &aCtx) noexcept;

    template <typename T>
    friend void details::ctx::reset_error(T &aCtx) noexcept;

   public:
    using error_type_list = utils::type_list<Error, RestErrors...>;
    using error_t = error<std::max({sizeof(Error), sizeof(RestErrors)...}),
                          std::max({alignof(Error), alignof(RestErrors)...})>;

   private:
    enum : std::uint8_t
    {
        kIsActive = 0b00000001,
        kHasError = 0b00000010
    };

    void move_error_helper(context &aCtx) noexcept
    {
        if (has_error())
        {
            details::ctx::reset_error(*this);
        }

        if (aCtx.has_error())
        {
            new (data_) error_t(std::move(aCtx.error()));
            this->set(kHasError);
            details::ctx::reset_error(aCtx);
        }
    }

   public:
    constexpr context() = default;
    context(const context &) = delete;
    context &operator=(const context &) = delete;

    context(context &&aCtx) noexcept { move_error_helper(aCtx); }

    context &operator=(context &&aCtx) noexcept
    {
        if (this != &aCtx)
        {
            move_error_helper(aCtx);
        }
        return *this;
    }

    error_t &error() noexcept
    {
        assert(has_error());
        return *reinterpret_cast<error_t *>(data_);
    }

    const error_t &error() const noexcept
    {
        assert(has_error());
        return *reinterpret_cast<error_t const *>(data_);
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

    inline bool is_active() const noexcept { return check(kIsActive); }

    ~context()
    {
        assert(!has_error() &&
               "error value and its payload will be lost and never has a "
               "chance to be handled");
    }

   protected:
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

   private:
    inline bool check(std::uint8_t aFlag) const noexcept
    {
        return state_ & aFlag;
    }

    inline void set(std::uint8_t aFlag) noexcept { state_ |= aFlag; }

    inline void reset(std::uint8_t aFlag) noexcept { state_ &= ~aFlag; }

    alignas(error_t) std::byte data_[sizeof(error_t)]{};
    std::uint8_t state_{};
};

template <typename Payload, typename... Errors>
class heavy_context final : public context<Errors...>
{
    using base = context<Errors...>;

   public:
    using payload_t = Payload;

    heavy_context(const heavy_context &) = delete;
    heavy_context &operator=(const heavy_context &) = delete;

    heavy_context(heavy_context &&) = default;
    heavy_context &operator=(heavy_context &&) = default;

    constexpr heavy_context(char *aPayload, std::size_t aSize) noexcept
        : base(), payload_(aPayload, aSize)
    {
    }

    template <std::size_t N>
    constexpr heavy_context(char (&aStorage)[N]) noexcept
        : heavy_context(aStorage, N)
    {
    }

    template <typename T>
    void load(T &&aArg) noexcept
    {
        assert(payload_ && "invalid payload_");
        payload_->load(std::forward<T>(aArg));
    }

    const payload_t &payload() const noexcept { return payload_; }
    payload_t &payload() noexcept { return payload_; }

   private:
    payload_t payload_;
};

namespace details
{
template <typename Context>
struct polymorphic_context_impl
    : polymorphic_context
    , Context
{
    bool is_active() const noexcept override final
    {
        return this == active_context_;
    }

    void activate() noexcept override final
    {
        Context::activate();
        active_context_ = this;
    }

    void deactivate() noexcept override final
    {
        active_context_ = nullptr;
        Context::deactivate();
    }
};
}  // namespace details

template <typename Ctx>
class context_activator
{
   public:
    context_activator(const context_activator &) = delete;
    context_activator &operator=(const context_activator &) = delete;
    context_activator &operator=(context_activator &&aOther) = delete;

    ~context_activator()
    {
        if (context_ && context_->is_active())
        {
            context_->deactivate();
        }
    }

    explicit inline context_activator(Ctx &aCtx) noexcept
        : context_{aCtx.is_active() ? nullptr : &aCtx}
    {
        if (context_)
        {
            context_->activate();
        }
    }

    inline context_activator(context_activator &&aOther) noexcept
        : context_{aOther.context_}
    {
        aOther.context_ = nullptr;
    }

   private:
    Ctx *context_{};
};
}  // namespace tricky

#endif /* tricky_context_h */
