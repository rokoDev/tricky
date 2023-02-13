#ifndef tricky_h
#define tricky_h

#include <interval/interval.h>
#include <user_literals/user_literals.h>
#include <utils/utils.h>

#include <memory>
#include <string_view>

#include "handlers.h"
#include "lazy_load.h"
#include "state.h"

#define TRICKY_SOURCE_LOCATION \
    ::tricky::e_source_location { __FILE__, __LINE__, __FUNCTION__ }

#define TRICKY_NEW_ERROR(E)       \
    {                             \
        E, TRICKY_SOURCE_LOCATION \
    }

#define TRICKY_TOKEN_PASTE(x, y) x##y
#define TRICKY_TOKEN_PASTE2(x, y) TRICKY_TOKEN_PASTE(x, y)
#define TRICKY_TMP TRICKY_TOKEN_PASTE2(tricky_tmp_, __LINE__)

#define TRICKY_ASSIGN(v, r)                                                   \
    auto &&TRICKY_TMP = r;                                                    \
    static_assert(tricky::is_result_v<std::decay_t<decltype(TRICKY_TMP)>>,    \
                  "second argument must be tricky::result<>. See is_result"); \
    if (!TRICKY_TMP) return TRICKY_TMP;                                       \
    v = std::forward<decltype(TRICKY_TMP)>(TRICKY_TMP).value()

#define TRICKY_AUTO(v, r) TRICKY_ASSIGN(auto v, r)

namespace tricky
{
namespace details
{
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
            rest_values.perform(aIndex - 1, std::forward<Action>(aAction));
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

enum class eDiscriminant
{
    kSuccess,
    kError,
    kInvalid
};
}  // namespace details

template <typename T, typename Error, typename... Errors>
class result
{
   private:
    template <typename U, typename E, typename... Es>
    friend class result;

    template <typename... Handlers>
    friend class details::handlers_base;

    using eDiscriminant = details::eDiscriminant;

    using shared_state = shared_state;

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

    template <typename E, typename... PayloadValue,
              typename = enable_if_valid_error_t<E>>
    inline result(E aError, PayloadValue &&...aValue) noexcept : error_(aError)
    {
        shared_state::enforce_value_state();
        shared_state::type_index(type_index_v<E>);
        (..., shared_state::load(std::forward<PayloadValue>(aValue)));
    }

    template <typename R,
              typename = std::enable_if_t<is_result_v<std::decay_t<R>>>>
    inline result(const R &&aResult) noexcept
    {
        using CoreT = std::decay_t<R>;
        static_assert(is_result_v<CoreT>);
        using other_value_type = typename CoreT::value_type;
        constexpr bool is_compatible = std::disjunction_v<
            std::is_same<other_value_type, value_type>,
            std::conjunction<std::is_same<value_type, details::void_>,
                             std::is_same<other_value_type, void>>>;
        if constexpr (is_compatible)
        {
            if (aResult.has_value())
            {
                value_ = aResult.value_;
            }
            else
            {
                init(std::move(aResult));
            }
        }
        else
        {
            shared_state::enforce_error_state();
            init(std::move(aResult));
        }
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
        if constexpr (std::conjunction_v<
                          std::is_same<U, void>,
                          std::is_same<value_type, details::void_>>)
        {
            constexpr std::size_t kIndex = type_index_v<value_type>;
            return kIndex == shared_state::type_index();
        }
        else
        {
            constexpr std::size_t kIndex = type_index_v<U>;
            return kIndex == shared_state::type_index();
        }
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

    template <typename... V>
    static void load(V &&...aValue) noexcept
    {
        static_assert(sizeof...(V) > 0);
        if (has_error())
        {
            (..., shared_state::load(std::forward<V>(aValue)));
        }
    }

   private:
    template <typename R>
    inline void init(const R &&aResult) noexcept
    {
        using errors_of_R = typename std::decay_t<R>::error_types;
        static_assert(error_types::template contains_v<errors_of_R>,
                      "errors of type R must be subset of <Error, Errors...>");
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
    eDiscriminant what_{eDiscriminant::kSuccess};
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

    template <typename E, typename... PayloadValue,
              typename = std::enable_if_t<std::is_enum_v<E>>>
    inline result(E aError, PayloadValue &&...aValue) noexcept
        : base(aError, std::forward<PayloadValue>(aValue)...)
    {
    }

    template <typename R,
              typename = std::enable_if_t<is_result_v<std::decay_t<R>>>>
    inline result(const R &&aResult) noexcept : base(std::move(aResult))
    {
    }

    inline void value() const noexcept
    {
        base::shared_state::enforce_value_state();
    }
};

template <typename... Callbacks>
constexpr bool process_payload(Callbacks &&...aCallbacks) noexcept
{
    return shared_state::get_payload().process(
        std::forward<Callbacks>(aCallbacks)...);
}

template <typename TryBlock, typename Handlers>
std::enable_if_t<
    std::conjunction_v<std::is_nothrow_invocable<TryBlock>,
                       is_handlers<utils::remove_cvref_t<Handlers>>>,
    typename std::decay_t<decltype(std::declval<TryBlock>()().value())>>
try_handle_all(TryBlock &&aTryBlock, Handlers &&aHandlers) noexcept
{
    using HandlersT = utils::remove_cvref_t<Handlers>;
    static_assert(is_result_v<decltype(std::declval<TryBlock>()())>,
                  "aTryBlock must return result<...> type");
    using value_type =
        std::decay_t<decltype(std::declval<TryBlock>()().value())>;
    using handlers_return_type = typename HandlersT::return_type;
    static_assert(std::is_same_v<value_type, handlers_return_type>);
    using handlers_list = typename HandlersT::handlers_list;
    static_assert(
        handlers_list::template count_of_predicate_compliant<is_any_handler> ==
            1,
        "Handlers must contain exactly one tricky::any_handler<T>.");
    if (auto r = std::forward<TryBlock>(aTryBlock)())
    {
        return std::move(r).value();
    }
    else
    {
        return aHandlers(std::move(r));
    }
}

template <typename TryBlock, typename Handlers>
std::enable_if_t<
    std::conjunction_v<std::is_nothrow_invocable<TryBlock>,
                       is_handlers<utils::remove_cvref_t<Handlers>>>,
    typename std::decay_t<decltype(std::declval<TryBlock>()())>>
try_handle_some(TryBlock &&aTryBlock, Handlers &&aHandlers) noexcept
{
    using HandlersT = utils::remove_cvref_t<Handlers>;
    using try_block_return_type =
        std::decay_t<decltype(std::declval<TryBlock>()())>;
    static_assert(is_result_v<try_block_return_type>,
                  "aTryBlock must return result<...> type");
    using handlers_return_type = typename HandlersT::return_type;
    static_assert(std::is_same_v<try_block_return_type, handlers_return_type>,
                  "aTryBlock and aHandlers must return the same type.");
    using handlers_list = typename HandlersT::handlers_list;
    static_assert(
        handlers_list::template count_of_predicate_compliant<is_any_handler> ==
            0,
        "Handlers must not contain any handlers of type "
        "tricky::any_handler<T>.");
    if (auto r = std::forward<TryBlock>(aTryBlock)())
    {
        return std::move(r);
    }
    else
    {
        return aHandlers(std::move(r));
    }
}
}  // namespace tricky

#endif /* tricky_h */
