#ifndef tricky_handlers_h
#define tricky_handlers_h

#include <utils/utils.h>

#include <type_traits>
#include <utility>

#include "state.h"

namespace tricky
{
template <typename T, typename Error, typename... Errors>
class result;

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

namespace details
{
template <typename Callable, typename... Es>
struct return_type_with_0_args
{
    using type = std::enable_if_t<std::is_nothrow_invocable_v<Callable>,
                                  std::invoke_result_t<Callable>>;
};

template <typename Callable, typename... Es>
struct return_type_with_1_args
{
    using type = std::enable_if_t<
        std::conjunction_v<std::is_nothrow_invocable<Callable, Es>...> &&
            utils::type_list<std::invoke_result_t<Callable, Es>...>::is_same,
        typename utils::type_list<
            std::invoke_result_t<Callable, Es>...>::template at<0>>;
};

template <typename Callable, typename... Es>
struct return_type
{
    template <typename Callable1, typename... Es1>
    static typename std::enable_if_t<std::is_nothrow_invocable_v<Callable1>,
                                     std::invoke_result_t<Callable1>>
    test();

    template <typename Callable1, typename... Es1>
    static typename std::enable_if_t<
        std::conjunction_v<std::is_nothrow_invocable<Callable1, Es1>...> &&
            utils::type_list<std::invoke_result_t<Callable1, Es1>...>::is_same,
        typename utils::type_list<
            std::invoke_result_t<Callable1, Es1>...>::template at<0>>
    test();

    using type = decltype(test<Callable, Es...>());
};
}  // namespace details
template <typename H, auto... Errors>
struct values_handler
{
    using handler_type = H;
    using error_categories = utils::type_list<decltype(Errors)...>;
    using error_values = utils::value_list<Errors...>;
    using return_type =
        typename details::return_type<H, decltype(Errors)...>::type;

    values_handler() = delete;
    inline constexpr values_handler(H &&aHandler) noexcept : handler(aHandler)
    {
        if constexpr (std::conjunction_v<
                          std::is_nothrow_invocable<H, decltype(Errors)>...>)
        {
            static_assert(
                utils::type_list<
                    std::invoke_result_t<H, decltype(Errors)>...>::is_same,
                "values_handler must return the same type for every error from "
                "'Errors...'");
        }
        static_assert(error_values::size > 0,
                      "number of Errors must be more than zero.");
        if constexpr (error_values::size == 1)
        {
            static_assert(
                std::disjunction_v<
                    std::conjunction<std::is_nothrow_invocable<H>>,
                    std::conjunction<
                        std::is_nothrow_invocable<H, decltype(Errors)>...>>,
                "aHandler must be nothrow invocable in one of forms: "
                "aHandler() or aHandler(aError)");
        }
        else
        {
            static_assert(
                std::conjunction_v<
                    std::is_nothrow_invocable<H, decltype(Errors)>...>,
                "aHandler must be nothrow invocable with any of Errors");
        }
        static_assert(not error_values::contains_copies,
                      "Errors must not contain copies.");
        static_assert(error_categories::template count_of_predicate_compliant<
                          std::is_enum> == error_categories::size,
                      "all Errors must belong to enum type.");
    }

    H handler;
};

template <typename T>
struct is_values_handler : std::false_type
{
};

template <typename H, auto... Errors>
struct is_values_handler<values_handler<H, Errors...>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_values_handler_v = is_values_handler<T>::value;

template <auto Error, auto... Errors, typename Handler>
constexpr decltype(auto) handler(Handler &&aHandler) noexcept
{
    return values_handler<Handler, Error, Errors...>(
        std::forward<Handler>(aHandler));
}

template <typename H, typename... Categories>
struct categories_handler
{
    using handler_type = H;
    using error_categories = utils::type_list<Categories...>;
    using return_type =
        typename details::return_type_with_1_args<H, Categories...>::type;

    categories_handler() = delete;
    inline constexpr categories_handler(H &&aHandler) noexcept
        : handler(aHandler)
    {
        static_assert(error_categories::size > 0,
                      "number of Categories... must be more than zero.");
        static_assert(
            std::conjunction_v<std::is_nothrow_invocable<H, Categories>...>,
            "aHandler must be nothrow invocable with any values that "
            "belong to any of the Categories...");
        static_assert(not error_categories::contains_copies,
                      "Categories... must not contain copies.");
        static_assert(error_categories::template count_of_predicate_compliant<
                          std::is_enum> == error_categories::size,
                      "every Categories... must be an enum.");
    }

    H handler;
};

template <typename T>
struct is_categories_handler : std::false_type
{
};

template <typename H, typename... Categories>
struct is_categories_handler<categories_handler<H, Categories...>>
    : std::true_type
{
};

template <typename T>
inline constexpr bool is_categories_handler_v = is_categories_handler<T>::value;

template <typename Category, typename... Categories, typename Handler>
constexpr decltype(auto) handler(Handler &&aHandler) noexcept
{
    return categories_handler<Handler, Category, Categories...>(
        std::forward<Handler>(aHandler));
}

namespace details
{
template <typename T, typename... A>
using call_noexcept_templated_functor =
    std::enable_if_t<noexcept(std::declval<T>().template operator()<A...>(
                         std::declval<A>()...)),
                     decltype(std::declval<T>().template operator()<A...>(
                         std::declval<A>()...))>;

template <typename T>
struct is_noexcept_templated_functor_with_one_arg
    : utils::is_detected<call_noexcept_templated_functor, T, int>
{
};
}  // namespace details

template <typename H>
struct any_handler
{
    using handler_type = H;
    using return_type =
        typename details::return_type_with_1_args<H, int8_t, int32_t>::type;

    any_handler() = delete;
    inline constexpr any_handler(H &&aHandler) noexcept : handler(aHandler)
    {
        static_assert(
            details::is_noexcept_templated_functor_with_one_arg<H>::value);
    }

    H handler;
};

template <typename T>
struct is_any_handler : std::false_type
{
};

template <typename H>
struct is_any_handler<any_handler<H>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_any_handler_v = is_any_handler<T>::value;

template <typename Handler>
constexpr decltype(auto) handler(Handler &&aHandler) noexcept
{
    return any_handler<Handler>(std::forward<Handler>(aHandler));
}

namespace details
{
template <typename T>
struct ret_type
{
    using type = typename T::return_type;
};

template <typename T>
using ret_type_t = typename ret_type<T>::type;

template <typename... Handlers>
class handlers_base : public Handlers...
{
   protected:
    using handlers_list = utils::type_list<Handlers...>;
    using return_type =
        typename utils::type_list<ret_type_t<Handlers>...>::template at<0>;

    template <auto Error>
    struct can_handle_error
    {
        template <typename H>
        struct impl : std::false_type
        {
        };

        template <typename H, auto... Errors>
        struct impl<values_handler<H, Errors...>>
            : std::bool_constant<
                  utils::value_list<Errors...>::template contains_v<Error>>
        {
        };
    };

    template <typename Category>
    struct can_handle_category
    {
        template <typename H>
        struct impl : std::false_type
        {
        };

        template <typename H, typename... Categories>
        struct impl<categories_handler<H, Categories...>>
            : std::bool_constant<utils::type_list<
                  Categories...>::template contains_v<Category>>
        {
        };
    };

    template <typename H>
    struct error_values_list
    {
        using type = utils::value_list<>;
    };

    template <typename H, auto... Errors>
    struct error_values_list<tricky::values_handler<H, Errors...>>
    {
        using type =
            typename tricky::values_handler<H, Errors...>::error_values;
    };

    template <typename H>
    using error_values_list_t = typename error_values_list<H>::type;

    using error_values = utils::concatenate_t<error_values_list_t<Handlers>...>;

    template <auto Error, typename R>
    constexpr return_type process_error_value(
        [[maybe_unused]] R &&aResult) const noexcept
    {
        using matched_handlers =
            typename handlers_list::template list_of_predicate_compliant_t<
                can_handle_error<Error>::template impl>;
        if constexpr (matched_handlers::size)
        {
            using value_handler = typename matched_handlers::template at<0>;
            if constexpr (std::is_same_v<return_type, void>)
            {
                if constexpr (std::conjunction_v<std::is_nothrow_invocable<
                                  typename value_handler::handler_type>>)
                {
                    value_handler::handler();
                }
                else
                {
                    value_handler::handler(Error);
                }
                tricky::shared_state::reset();
            }
            else
            {
                return_type retVal{};
                if constexpr (std::conjunction_v<std::is_nothrow_invocable<
                                  typename value_handler::handler_type>>)
                {
                    retVal = value_handler::handler();
                }
                else
                {
                    retVal = value_handler::handler(Error);
                }
                tricky::shared_state::reset();
                return std::move(retVal);
            }
        }
        else
        {
            return process_error_category(std::forward<R>(aResult), Error);
        }
    }

    template <typename R, typename Category>
    constexpr return_type process_error_category([[maybe_unused]] R &&aResult,
                                                 Category aError) const noexcept
    {
        using matched_handlers =
            typename handlers_list::template list_of_predicate_compliant_t<
                can_handle_category<Category>::template impl>;
        if constexpr (matched_handlers::size)
        {
            using category_handler = typename matched_handlers::template at<0>;
            if constexpr (std::is_same_v<return_type, void>)
            {
                category_handler::handler(aError);
                tricky::shared_state::reset();
            }
            else
            {
                auto retVal = category_handler::handler(aError);
                tricky::shared_state::reset();
                return std::move(retVal);
            }
        }
        else
        {
            return process_any_error(std::forward<R>(aResult), aError);
        }
    }

    template <typename R, typename Category>
    constexpr return_type process_any_error(
        [[maybe_unused]] R &&aResult,
        [[maybe_unused]] Category aError) const noexcept
    {
        using matched_handlers =
            typename handlers_list::template list_of_predicate_compliant_t<
                is_any_handler>;
        if constexpr (matched_handlers::size)
        {
            using any_error_handler = typename matched_handlers::template at<0>;
            if constexpr (std::is_same_v<return_type, void>)
            {
                any_error_handler::handler(aError);
                tricky::shared_state::reset();
            }
            else
            {
                auto retVal = any_error_handler::handler(aError);
                tricky::shared_state::reset();
                return std::move(retVal);
            }
        }
        else
        {
            return std::move(aResult);
        }
    }

    template <typename Result>
    struct value_handler_func
    {
        using type = return_type (handlers_base::*)(Result &&) const noexcept;
    };

    template <typename E, typename IntervalT, typename R, std::size_t... I>
    constexpr return_type call_value_handler(
        R &&aResult, std::size_t aIndex,
        std::index_sequence<I...>) const noexcept
    {
        using value_handler_func_t = typename value_handler_func<R>::type;
        value_handler_func_t handlers[sizeof...(I)]{
            &handlers_base::process_error_value<
                static_cast<E>(IntervalT::valueAt(I)), R>...};
        return (this->*handlers[aIndex])(std::forward<R>(aResult));
    }

    handlers_base() = delete;
    inline constexpr handlers_base(Handlers &&...aHandlers) noexcept
        : Handlers(std::forward<Handlers>(aHandlers))...
    {
        static_assert(utils::type_list<ret_type_t<Handlers>...>::is_same,
                      "All handlers must have identical return type.");
    }

    template <typename E, typename R>
    constexpr return_type process_error_in_result(R &&aResult) const noexcept
    {
        static_assert(not error_values::contains_copies,
                      "duplications of error values is not allowed.");

        const auto kError = aResult.template error<E>();
        if constexpr (error_values::template contains_type<E>)
        {
            constexpr E kMin = error_values::template min<E>;
            constexpr E kMax = error_values::template max<E>;

            using interval_type =
                interval::Interval<interval::Min<utils::to_underlying(kMin)>,
                                   interval::Max<utils::to_underlying(kMax)>>;
            constexpr std::size_t kCount = interval_type::kMaxIndex + 1_uz;
            const auto kErrorAsIntegral = utils::to_underlying(kError);

            if (interval_type::contains(kErrorAsIntegral))
            {
                const std::size_t kIndex =
                    interval_type::indexOf(kErrorAsIntegral);
                return call_value_handler<E, interval_type>(
                    std::forward<R>(aResult), kIndex,
                    std::make_index_sequence<kCount>{});
            }
            else
            {
                return process_error_category(std::forward<R>(aResult), kError);
            }
        }
        else
        {
            return process_error_category(std::forward<R>(aResult), kError);
        }
    }

    template <typename R, std::size_t... I>
    constexpr return_type process_impl(R &&aResult,
                                       std::index_sequence<I...>) const noexcept
    {
        using ResultT = utils::remove_cvref_t<R>;
        static_assert(is_result_v<ResultT>);

        if constexpr (handlers_list::template contains_predicate_compliant<
                          is_any_handler>)
        {
            static_assert(handlers_list::template count_of_predicate_compliant<
                              is_any_handler> == 1);
            static_assert(
                std::is_same_v<return_type, typename ResultT::value_type>);
        }
        else
        {
            static_assert(std::is_same_v<return_type, ResultT>);
        }

        if (aResult.has_error())
        {
            using value_handler_func_t = typename value_handler_func<R>::type;
            constexpr value_handler_func_t callbacks[sizeof...(I)] = {
                &handlers_base::process_error_in_result<
                    typename ResultT::error_types::template at<I>,
                    decltype(aResult)>...};
            return (this->*callbacks[tricky::shared_state::type_index() - 1])(
                std::forward<R>(aResult));
        }
        else
        {
            if constexpr (not std::is_same_v<return_type, void>)
            {
                return {aResult.value()};
            }
        }
    }
};
}  // namespace details

template <typename... Handlers>
class handlers final : protected details::handlers_base<Handlers...>
{
    using base = details::handlers_base<Handlers...>;

   public:
    using handlers_list = typename base::handlers_list;
    using return_type = typename base::return_type;
    handlers() = delete;

    inline constexpr handlers(Handlers &&...aHandlers) noexcept
        : base(std::forward<Handlers>(aHandlers)...)
    {
        static_assert(
            std::conjunction_v<std::disjunction<is_values_handler<Handlers>,
                                                is_categories_handler<Handlers>,
                                                is_any_handler<Handlers>>...>,
            "Every handler should belong to one of the types: values_handler, "
            "categories_handler or any_handler.");
    }

    template <typename R>
    constexpr decltype(auto) operator()(R &&aResult) const noexcept
    {
        static_assert(not std::is_lvalue_reference_v<std::remove_const_t<R>>,
                      "aResult is not allowed to be l-value reference");
        using ResultT = utils::remove_cvref_t<R>;
        constexpr std::size_t num_error_types = ResultT::type_count - 1;
        using Indices = std::make_index_sequence<num_error_types>;
        return base::process_impl(std::forward<R>(aResult), Indices{});
    }
};

template <typename T>
struct is_handlers : std::false_type
{
};

template <typename... Handlers>
struct is_handlers<handlers<Handlers...>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_handlers_v = is_handlers<T>::value;
}  // namespace tricky

#endif /* tricky_handlers_h */
