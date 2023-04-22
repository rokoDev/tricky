#ifndef tricky_context_h
#define tricky_context_h

#include <utils/utils.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>

#include "payload.h"

namespace tricky
{
template <typename Payload, typename... Errors>
class context
{
   public:
    using error_type_list = utils::type_list<Errors...>;
    using payload_t = Payload;
    static constexpr std::size_t kMaxSizeOfError{std::max({sizeof(Errors)...})};
    using Indices = std::make_index_sequence<kMaxSizeOfError>;

   private:
    using error_destroyer_t = void (context<Payload, Errors...>::*)() noexcept;

    template <typename U>
    void destroy_error() noexcept
    {
        static_assert(error_type_list::template contains_v<U>);
        reinterpret_cast<U *>(data_)->~U();
    }

    template <typename Error>
    void set_error(Error aError) noexcept
    {
        static_assert(std::is_nothrow_copy_constructible_v<Error>);
        reset();
        new (data_) Error(aError);
        error_destroyer_ = &context<Payload, Errors...>::destroy_error<Error>;
    }

    void reset() noexcept
    {
        if (error_destroyer_)
        {
            (this->*error_destroyer_)();
            error_destroyer_ = nullptr;
            payload_->reset();
        }
    }

    template <std::size_t... I>
    context(context &&aCtx, std::index_sequence<I...>) noexcept
        : error_destroyer_{aCtx.error_destroyer_}
        , data_{aCtx.data_[I]...}
        , payload_{aCtx.payload_}
    {
        aCtx.error_destroyer_ = nullptr;
        aCtx.payload_ = nullptr;
    }

   public:
    context(const context &) = delete;
    context &operator=(const context &) = delete;

    context(context &&aCtx) noexcept : context(std::move(aCtx), Indices{}) {}

    context &operator=(context &&aCtx) noexcept
    {
        if (this != &aCtx)
        {
            std::memmove(data_, aCtx.data_, kMaxSizeOfError);
            error_destroyer_ = aCtx.error_destroyer_;
            payload_ = aCtx.payload_;
            aCtx.error_destroyer_ = nullptr;
            aCtx.payload_ = nullptr;
        }
        return *this;
    }

    constexpr context(Payload &aPayload) noexcept : payload_(&aPayload)
    {
        static_assert(is_payload_v<Payload>);
        static_assert(sizeof...(Errors) > 0);
    }

    template <typename T>
    void load(T &&aArg) noexcept
    {
        assert(error_destroyer_ &&
               "you can not add payload to a context if error is not set");
        assert(payload_ && "invalid payload_");
        payload_->load(std::forward<T>(aArg));
    }

    bool is_moved() noexcept { return !payload_; }

    bool is_error() noexcept { return error_destroyer_; }

    const payload_t *payload() noexcept { return payload_; }

    ~context() { reset(); }

   private:
    error_destroyer_t error_destroyer_{nullptr};
    alignas(Errors...) std::byte data_[kMaxSizeOfError]{};
    payload_t *payload_{nullptr};
};
}  // namespace tricky

#endif /* tricky_context_h */
