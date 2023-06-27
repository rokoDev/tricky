#ifndef tricky_context_h
#define tricky_context_h

#include <strong_type/strong_type.h>
#include <utils/utils.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>

namespace tricky
{
template <typename Payload, typename... Errors>
class context;

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

template <typename E, typename Context>
static void destroy_error(Context &aCtx) noexcept
{
    aCtx.template reset_error<E>();
}

template <typename E>
static void copy_error(Dst aDst, Src aSrc) noexcept
{
    static_assert(std::is_nothrow_copy_constructible_v<E>);
    assert(utils::is_aligned<E>(aDst) &&
           "aDst is not aligned to contain value of type E");
    assert(utils::is_aligned<E>(aSrc) &&
           "aSrc is not aligned to contain value of type E");
    new (aDst) E(*reinterpret_cast<E const *>(aSrc));
}
}  // namespace error_ops
}  // namespace details

template <typename Payload, typename... Errors>
class context
{
   public:
    using error_type_list = utils::type_list<Errors...>;
    using payload_t = Payload;

   private:
    using Src = details::error_ops::Src;
    using Dst = details::error_ops::Dst;

    using error_destroyer_t = details::error_ops::error_destroyer_t<context>;
    using error_copier_t = details::error_ops::error_copier_t;

    template <typename E>
    void set_error(E aError) noexcept
    {
        static_assert(std::is_nothrow_copy_constructible_v<E>);
        static_assert(error_type_list::template contains_v<E>);
        static_assert(std::is_nothrow_destructible_v<E>);
        assert(!error_destroyer_ &&
               "error will be lost and never has a chance to be handled");
        assert(!error_copier_ &&
               "error will be lost and never has a chance to be handled");
        new (data_) E(aError);
        error_destroyer_ = details::error_ops::destroy_error<E, context>;
        error_copier_ = details::error_ops::copy_error<E>;
        error_index_ = error_type_list::template first_index_of_type<E>;
    }

    template <typename E>
    void reset_error() noexcept
    {
        if (error_destroyer_)
        {
            assert(error_copier_);
            static_assert(error_type_list::template contains_v<E>);
            reinterpret_cast<E *>(data_)->~E();
            error_destroyer_ = nullptr;
            error_copier_ = nullptr;
            error_index_ = error_type_list::size;
            if (payload_)
            {
                payload_->reset();
            }
        }
        else
        {
            assert(!error_copier_);
            assert(!payload_ || (payload_ && !payload_->size()));
        }
    }

    void reset_error() noexcept { std::invoke(error_destroyer_, *this); }

    void move_error_helper(context &aCtx) noexcept
    {
        aCtx.payload_ = nullptr;
        if (error_copier_)
        {
            assert(error_destroyer_);

            // copy error value from aCtx.data_ to data_
            std::invoke(error_copier_, Dst(data_), Src(aCtx.data_));

            // destroy error value in aCtx
            aCtx.reset_error();
        }
        else
        {
            assert(!error_destroyer_);
        }
    }

   public:
    context() noexcept = default;

    context(const context &) = delete;
    context &operator=(const context &) = delete;

    context(context &&aCtx) noexcept
        : error_destroyer_{aCtx.error_destroyer_}
        , error_copier_{aCtx.error_copier_}
        , payload_{aCtx.payload_}
        , data_{}
    {
        move_error_helper(aCtx);
    }

    context &operator=(context &&aCtx) noexcept
    {
        if (this != &aCtx)
        {
            assert(!error_destroyer_ &&
                   "error will be lost and never has a chance to be handled");
            assert(!error_copier_ &&
                   "error will be lost and never has a chance to be handled");
            error_destroyer_ = aCtx.error_destroyer_;
            error_copier_ = aCtx.error_copier_;
            payload_ = aCtx.payload_;
            move_error_helper(aCtx);
        }
        return *this;
    }

    constexpr context(Payload *aPayload) noexcept : payload_(aPayload)
    {
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

    template <typename E>
    std::enable_if_t<error_type_list::template contains_v<E>, bool> has_error()
        const noexcept
    {
        return error_type_list::template first_index_of_type<E> == error_index_;
    }

    inline bool has_error() const noexcept { return error_destroyer_; }

    template <typename E>
    std::enable_if_t<error_type_list::template contains_v<E>, E> get_error()
        const noexcept
    {
        assert(has_error<E>());
        return *reinterpret_cast<E const *>(data_);
    }

    payload_t const *payload() const noexcept { return payload_; }
    payload_t *payload() noexcept { return payload_; }

    inline bool is_active() const noexcept { return is_active_; }

    void activate() noexcept
    {
        assert(!is_active_);
        is_active_ = true;
    }

    void deactivate() noexcept
    {
        assert(is_active_);
        is_active_ = false;
    }

    ~context()
    {
        assert(!has_error() &&
               "error value and its payload will be lost and never has a "
               "chance to be handled");
    }

   private:
    error_destroyer_t error_destroyer_{nullptr};
    error_copier_t error_copier_{nullptr};
    payload_t *payload_{nullptr};
    std::size_t error_index_{error_type_list::size};
    alignas(Errors...) std::byte data_[std::max({sizeof(Errors)...})]{};
    bool is_active_{};
};
}  // namespace tricky

#endif /* tricky_context_h */
