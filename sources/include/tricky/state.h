#ifndef tricky_state_h
#define tricky_state_h
#include "payload.h"

namespace tricky
{
#ifdef TRICKY_PAYLOAD_MAXSPACE
inline constexpr std::size_t kPayloadMaxSpace = TRICKY_PAYLOAD_MAXSPACE;
#else
inline constexpr std::size_t kPayloadMaxSpace = 256;
#endif

#ifdef TRICKY_PAYLOAD_MAXCOUNT
inline constexpr std::size_t kPayloadMaxCount = TRICKY_PAYLOAD_MAXCOUNT;
#else
inline constexpr std::size_t kPayloadMaxCount = 16;
#endif

namespace details
{
template <std::size_t MaxSpace, std::size_t MaxCount>
class state
{
   public:
    void reset() noexcept
    {
        type_index_ = 0;
        payload_.reset();
    }
    inline constexpr bool has_error() const noexcept { return type_index_; }
    inline constexpr bool has_value() const noexcept { return !has_error(); }

    inline constexpr void enforce_error_state() const noexcept
    {
        assert(has_error() && "state must contain an error.");
    }

    inline constexpr void enforce_value_state() const noexcept
    {
        assert(has_value() &&
               "state must be clear. It looks like you are trying to "
               "construct result<...> with error state without handling "
               "previous result<...> with error state.");
    }

    void type_index(std::size_t aIndex) noexcept { type_index_ = aIndex; }

    inline constexpr std::size_t type_index() const noexcept
    {
        return type_index_;
    }

    inline constexpr const payload<MaxSpace, MaxCount> &get_payload()
        const noexcept
    {
        return payload_;
    }

    inline constexpr payload<MaxSpace, MaxCount> &get_payload() noexcept
    {
        return payload_;
    }

   private:
    std::size_t type_index_{};
    payload<MaxSpace, MaxCount> payload_;
};

template <std::size_t MaxSpace, std::size_t MaxCount>
class shared_state
{
   public:
    using state = state<MaxSpace, MaxCount>;

    static void reset() noexcept { state_.reset(); }
    static constexpr bool has_error() noexcept { return type_index(); }
    static constexpr bool has_value() noexcept { return !has_error(); }

    static inline constexpr void enforce_error_state() noexcept
    {
        assert(has_error() && "state must contain an error.");
    }

    static inline constexpr void enforce_value_state() noexcept
    {
        assert(has_value() &&
               "state must be clear. It looks like you are trying to "
               "construct result<...> with error state without handling "
               "previous result<...> with error state.");
    }

    static void type_index(std::size_t aIndex) noexcept
    {
        state_.type_index(aIndex);
    }

    static constexpr std::size_t type_index() noexcept
    {
        return state_.type_index();
    }

    static constexpr const payload<MaxSpace, MaxCount> &get_payload() noexcept
    {
        return state_.get_payload();
    }

    template <typename T>
    static void load(T &&aValue) noexcept
    {
        state_.get_payload().load(std::forward<T>(aValue));
    }

   private:
    static state state_;
};

template <std::size_t MaxSpace, std::size_t MaxCount>
state<MaxSpace, MaxCount> shared_state<MaxSpace, MaxCount>::state_{};
}  // namespace details

using shared_state = details::shared_state<kPayloadMaxSpace, kPayloadMaxCount>;
}  // namespace tricky

#endif /* tricky_state_h */
