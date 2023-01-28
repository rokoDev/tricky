#ifndef tricky_h
#define tricky_h

#include <interval/interval.h>
#include <user_literals/user_literals.h>
#include <utils/utils.h>

#include <memory>
#include <string_view>

#include "handlers.h"
#include "payload.h"

#define TRICKY_SOURCE_LOCATION \
    ::tricky::e_source_location { __FILE__, __LINE__, __FUNCTION__ }

#define TRICKY_NEW_ERROR(E)       \
    {                             \
        E, TRICKY_SOURCE_LOCATION \
    }

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

template <typename T, typename Error, typename... Errors>
class result;

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

using result_state = shared_state<kPayloadMaxSpace, kPayloadMaxCount>;

template <class T>
struct stored
{
    using type = T;
    using value_type = T;
    using value_type_const = T const;
    using value_cref = T const &;
    using value_ref = T &;
    using value_rv_cref = T const &&;
    using value_rv_ref = T &&;
};

struct void_
{
};

template <typename E, typename... Es>
union any_error
{
    E value;
    any_error<Es...> rest_values;

    template <typename T, typename Error, typename... Errors>
    inline constexpr result<T, Error, Errors...> make_result(
        [[maybe_unused]] std::size_t aIndex) const noexcept
    {
        if (aIndex)
        {
            return rest_values.template make_result<T, Error, Errors...>(
                aIndex - 1);
        }
        else
        {
            return result<T, Error, Errors...>{value};
        }
    }

    template <typename Action>
    constexpr void perform(std::size_t aIndex, Action &&aAction) const noexcept
    {
        if (aIndex)
        {
            perform(aIndex - 1, std::forward<Action>(aAction));
        }
        else
        {
            aAction(value);
        }
    }

    inline constexpr any_error() noexcept : value() {}

    inline constexpr any_error(const any_error &) = default;
    inline constexpr any_error &operator=(const any_error &) noexcept = default;

    inline constexpr any_error &operator=(any_error &&) noexcept = default;
    inline constexpr any_error(any_error &&) noexcept = default;

    template <typename Error>
    inline constexpr any_error(Error aValue) noexcept
        : any_error(
              [aValue]
              {
                  if constexpr (std::is_same_v<Error, E>)
                  {
                      return E{aValue};
                  }
                  else
                  {
                      return any_error<Es...>{aValue};
                  }
              }(),
              void_{})
    {
    }

   private:
    inline constexpr any_error(E aValue, void_) noexcept : value(aValue) {}

    inline constexpr any_error(any_error<Es...> aRestValues, void_) noexcept
        : rest_values(aRestValues)
    {
    }
};

template <typename E>
union any_error<E>
{
    E value;

    template <typename T, typename Error, typename... Errors>
    inline constexpr result<T, Error, Errors...> make_result(
        [[maybe_unused]] std::size_t aIndex) const noexcept
    {
        return result<T, Error, Errors...>{value};
    }

    template <typename Action>
    constexpr void perform([[maybe_unused]] std::size_t aIndex,
                           Action &&aAction) const noexcept
    {
        assert((aIndex == 0) && "invalid index");
        aAction(value);
    }

    inline constexpr any_error() noexcept : value() {}

    inline constexpr any_error(const any_error &) = default;
    inline constexpr any_error &operator=(const any_error &) noexcept = default;

    inline constexpr any_error &operator=(any_error &&) noexcept = default;
    inline constexpr any_error(any_error &&) noexcept = default;

    inline constexpr any_error(E aValue) noexcept : value(aValue) {}
};
}  // namespace details

template <typename T>
struct is_result : std::false_type
{
};

template <typename T, typename Error, typename... Errors>
struct is_result<result<T, Error, Errors...>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_result_v = is_result<T>::value;

template <typename T, typename Error, typename... Errors>
class result
{
   private:
    template <typename U, typename E, typename... Es>
    friend class result;

    template <typename... Handlers>
    friend class details::handlers_base;

    using shared_state = details::result_state;

    using error_types = utils::type_list<Error, Errors...>;

    using all_types = utils::type_list<T, Error, Errors...>;

   public:
    static constexpr std::size_t type_count = sizeof...(Errors) + 2_uz;
    using index_t = utils::uint_from_nbits_t<utils::bits_count(type_count)>;

   private:
    template <typename U>
    static constexpr std::size_t type_index_v =
        all_types::template first_index_of_type<U>;

    template <typename E, class R = void>
    using enable_if_valid_error_t = std::enable_if_t<
        std::conjunction_v<
            std::is_enum<E>,
            std::bool_constant<type_index_v<E> != all_types::size>>,
        R>;

    using R = result<T, Error, Errors...>;

    template <typename U>
    using stored_type = typename details::stored<U>::type;

    template <typename U>
    using value_type_const = typename details::stored<U>::value_type_const;

    template <typename U>
    using value_ref = typename details::stored<U>::value_ref;

    template <typename U>
    using value_cref = typename details::stored<U>::value_cref;

    template <typename U>
    using value_rv_ref = typename details::stored<U>::value_rv_ref;

    template <typename U>
    using value_rv_cref = typename details::stored<U>::value_rv_cref;

   public:
    using value_type = typename details::stored<T>::value_type;

    ~result() = default;
    inline constexpr result() noexcept : result(T{}) {}

    inline constexpr result(const result &aOther) noexcept
    {
        shared_state::enforce_value_state();
        value_ = aOther.value_;
    }

    inline constexpr result &operator=(const result &aOther) noexcept
    {
        shared_state::enforce_value_state();
        if (this != &aOther)
        {
            value_ = aOther.value_;
        }
        return *this;
    }

    inline constexpr result &operator=(result &&) noexcept = default;
    inline constexpr result(result &&) noexcept = default;

    inline constexpr result(T aValue) noexcept : value_(aValue) {}

    template <typename E, typename = enable_if_valid_error_t<E>>
    inline result(E aError) noexcept : error_(aError)
    {
        shared_state::enforce_value_state();
        shared_state::type_index(type_index_v<E>);
    }

    template <typename E, typename PayloadValue,
              typename = enable_if_valid_error_t<E>>
    inline result(E aError, PayloadValue &&aValue) noexcept : result(aError)
    {
        load(std::forward<PayloadValue>(aValue));
    }

    template <typename E, typename... Es>
    inline result(const result<T, E, Es...> &&aResult) noexcept
    {
        using ResultT = decltype(aResult);
        init(std::forward<ResultT>(aResult));
    }

    explicit operator bool() const noexcept
    {
        return shared_state::has_value();
    }

    static inline bool has_error() noexcept
    {
        return shared_state::has_error();
    }

    static inline bool has_value() noexcept
    {
        return shared_state::has_value();
    }

    inline value_cref<T> value() const &noexcept
    {
        shared_state::enforce_value_state();
        return value_;
    }

    inline value_ref<T> value() &noexcept
    {
        shared_state::enforce_value_state();
        return value_;
    }

    inline value_rv_cref<T> value() const &&noexcept
    {
        shared_state::enforce_value_state();
        return std::move(value_);
    }

    inline value_rv_ref<T> value() &&noexcept
    {
        shared_state::enforce_value_state();
        return std::move(value_);
    }

    template <typename U>
    inline bool is_active_type() const noexcept
    {
        constexpr std::size_t kIndex = type_index_v<U>;
        return kIndex == shared_state::type_index();
    }

    template <typename E>
    inline value_cref<E> error() const &noexcept
    {
        enforce_error_type<E>();
        return error_at<E, type_index_v<E> - 1>(error_);
    }

    template <typename E>
    inline value_ref<E> error() &noexcept
    {
        enforce_error_type<E>();
        return error_at<E, type_index_v<E> - 1>(error_);
    }

    template <typename E>
    inline value_rv_cref<E> error() const &&noexcept
    {
        enforce_error_type<E>();
        return std::move(*this).template error_at<E, type_index_v<E> - 1>(
            error_);
    }

    template <typename E>
    inline value_rv_ref<E> error() &&noexcept
    {
        enforce_error_type<E>();
        return std::move(*this).template error_at<E, type_index_v<E> - 1>(
            error_);
    }

    template <typename V>
    static void load(V &&aValue) noexcept
    {
        shared_state::load(std::forward<V>(aValue));
    }

   private:
    template <typename E, typename... Es>
    inline void init(const result<T, E, Es...> &&aResult) noexcept
    {
        static_assert(
            std::conjunction_v<typename error_types::template contains<E>,
                               typename error_types::template contains<Es>...>,
            "<E, Es...> must be subset of <Error, Errors...>");
        if (shared_state::has_value())
        {
            value_ = aResult.value_;
        }
        else
        {
            aResult.error_.perform(
                shared_state::type_index() - 1,
                [this](auto aError)
                {
                    using ActiveType = decltype(aError);
                    error_at<ActiveType, type_index_v<ActiveType> - 1>(error_) =
                        aError;
                    shared_state::type_index(type_index_v<ActiveType>);
                });
        }
    }

    template <typename E, std::size_t I, typename AnyE>
    inline constexpr value_cref<E> error_at(AnyE &&aError) const &noexcept
    {
        if constexpr (I)
        {
            return error_at<E, I - 1>(aError.rest_values);
        }
        else
        {
            return aError.value;
        }
    }

    template <typename E, std::size_t I, typename AnyE>
    inline constexpr value_ref<E> error_at(AnyE &&aError) &noexcept
    {
        if constexpr (I)
        {
            return error_at<E, I - 1>(aError.rest_values);
        }
        else
        {
            return aError.value;
        }
    }

    template <typename E, std::size_t I, typename AnyE>
    inline constexpr value_rv_cref<E> error_at(AnyE &&aError) const &&noexcept
    {
        if constexpr (I)
        {
            return std::move(*this).template error_at<E, I - 1>(
                aError.rest_values);
        }
        else
        {
            return std::move(aError.value);
        }
    }

    template <typename E, std::size_t I, typename AnyE>
    inline constexpr value_rv_ref<E> error_at(AnyE &&aError) &&noexcept
    {
        if constexpr (I)
        {
            return std::move(*this).template error_at<E, I - 1>(
                aError.rest_values);
        }
        else
        {
            return std::move(aError.value);
        }
    }

    template <typename E>
    inline void enforce_error_type() const noexcept
    {
        shared_state::enforce_error_state();
        assert(
            is_active_type<E>() &&
            "type of value stored in this result object is different than E.");
    }

    union
    {
        stored_type<T> value_;
        details::any_error<Error, Errors...> error_;
    };
};

template <typename Error, typename... Errors>
class result<void, Error, Errors...>
    : public result<details::void_, Error, Errors...>
{
    template <typename U, typename E, typename... Es>
    friend class result;

    template <typename... Handlers>
    friend class handlers_base;

    using void_ = details::void_;
    using base = result<void_, Error, Errors...>;

   public:
    using base::operator bool;
    using value_type = void;

    ~result() = default;
    inline constexpr result() noexcept = default;

    inline constexpr result(const result &aOther) noexcept : base(aOther) {}

    inline constexpr result(result &&) noexcept = default;
    inline constexpr result &operator=(const result &aOther) noexcept
    {
        base::operator=(aOther);
        return *this;
    }

    inline constexpr result &operator=(result &&) noexcept = default;

    template <typename E>
    inline result(E aError) noexcept : base(aError)
    {
    }

    template <typename E, typename... Es>
    inline result(const result<void, E, Es...> &&aResult) noexcept
    {
        using R = decltype(aResult);
        base::init(std::forward<R>(aResult));
    }

    inline void value() const noexcept { base::enforce_value_state(); }
};

template <typename... Callbacks>
constexpr bool process_payload(Callbacks &&...aCallbacks) noexcept
{
    return details::result_state::get_payload().process(
        std::forward<Callbacks>(aCallbacks)...);
}
}  // namespace tricky

#endif /* tricky_h */
