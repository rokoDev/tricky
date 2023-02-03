#ifndef tricky_handlers_h
#define tricky_handlers_h

#include <utils/utils.h>

#include <type_traits>
#include <utility>

#include "state.h"

namespace tricky
{
template <typename H, auto... Errors>
struct values_handler
{
    using handler_type = H;
    using error_categories = utils::type_list<decltype(Errors)...>;
    using error_values = utils::value_list<Errors...>;

    values_handler() = delete;
    inline constexpr values_handler(H &&aHandler) noexcept : handler(aHandler)
    {
        static_assert(error_values::size > 0,
                      "number of Errors must be more than zero.");
        if constexpr (error_values::size == 1)
        {
            static_assert(
                std::disjunction_v<
                    std::conjunction<std::is_nothrow_invocable_r<int, H>>,
                    std::conjunction<std::is_nothrow_invocable_r<
                        int, H, decltype(Errors)>...>>,
                "aHandler must be nothrow invocable in one of forms: "
                "aHandler() or aHandler(aError)");
        }
        else
        {
            static_assert(
                std::conjunction_v<
                    std::is_nothrow_invocable_r<int, H, decltype(Errors)>...>,
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

    categories_handler() = delete;
    inline constexpr categories_handler(H &&aHandler) noexcept
        : handler(aHandler)
    {
        static_assert(error_categories::size > 0,
                      "number of Categories... must be more than zero.");
        static_assert(std::conjunction_v<
                          std::is_nothrow_invocable_r<int, H, Categories>...>,
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

template <typename H>
struct any_handler
{
    using handler_type = H;

    any_handler() = delete;
    inline constexpr any_handler(H &&aHandler) noexcept : handler(aHandler) {}

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
template <typename... Handlers>
class handlers_base : public Handlers...
{
   protected:
    using handlers_list = utils::type_list<Handlers...>;

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

    template <auto Error>
    constexpr int process_error_value() const noexcept
    {
        using matched_handlers =
            typename handlers_list::template list_of_predicate_compliant_t<
                can_handle_error<Error>::template impl>;
        if constexpr (matched_handlers::size)
        {
            using value_handler = typename matched_handlers::template at<0>;
            int retVal{};
            if constexpr (std::conjunction_v<std::is_nothrow_invocable_r<
                              void, typename value_handler::handler_type>>)
            {
                retVal = value_handler::handler();
            }
            else
            {
                retVal = value_handler::handler(Error);
            }
            tricky::shared_state::reset();
            return retVal;
        }
        else
        {
            return process_error_category(Error);
        }
    }

    template <typename Category>
    constexpr int process_error_category(Category aError) const noexcept
    {
        using matched_handlers =
            typename handlers_list::template list_of_predicate_compliant_t<
                can_handle_category<Category>::template impl>;
        if constexpr (matched_handlers::size)
        {
            using category_handler = typename matched_handlers::template at<0>;
            int retVal = category_handler::handler(aError);
            tricky::shared_state::reset();
            return retVal;
        }
        else
        {
            return process_any_error(aError);
        }
    }

    template <typename Category>
    constexpr int process_any_error(
        [[maybe_unused]] Category aError) const noexcept
    {
        using matched_handlers =
            typename handlers_list::template list_of_predicate_compliant_t<
                is_any_handler>;
        int retVal{-1};
        if constexpr (matched_handlers::size)
        {
            using any_error_handler = typename matched_handlers::template at<0>;
            retVal = any_error_handler::handler(aError);
            tricky::shared_state::reset();
        }
        return retVal;
    }

    using value_handler_func_t = int (handlers_base::*)() const noexcept;

    template <typename E, typename IntervalT, std::size_t... I>
    constexpr int call_value_handler(std::size_t aIndex,
                                     std::index_sequence<I...>) const noexcept
    {
        value_handler_func_t handlers[sizeof...(I)]{
            &handlers_base::process_error_value<static_cast<E>(
                IntervalT::valueAt(I))>...};
        return (this->*handlers[aIndex])();
    }

    handlers_base() = delete;
    inline constexpr handlers_base(Handlers &&...aHandlers) noexcept
        : Handlers(std::forward<Handlers>(aHandlers))...
    {
    }

    template <typename R>
    using handler_func_t = int (handlers_base::*)(R &&aResult) const noexcept;

    template <typename E, typename R>
    constexpr int process_error_in_result(R &&aResult) const noexcept
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
                    kIndex, std::make_index_sequence<kCount>{});
            }
            else
            {
                return process_error_category(kError);
            }
        }
        else
        {
            return process_error_category(kError);
        }
    }

    template <typename R, std::size_t... I>
    constexpr int process_impl(R &&aResult,
                               std::index_sequence<I...>) const noexcept
    {
        using ResultT = utils::remove_cvref_t<R>;
        int retVal{};
        if (aResult.has_error())
        {
            constexpr handler_func_t<R> callbacks[sizeof...(I)] = {
                &handlers_base::process_error_in_result<
                    typename ResultT::error_types::template at<I>, R>...};
            retVal = (this->*callbacks[tricky::shared_state::type_index() - 1])(
                std::forward<R>(aResult));
        }
        return retVal;
    }
};
}  // namespace details

template <typename... Handlers>
class handlers final : protected details::handlers_base<Handlers...>
{
   public:
    handlers() = delete;

    inline constexpr handlers(Handlers &&...aHandlers) noexcept
        : details::handlers_base<Handlers...>(
              std::forward<Handlers>(aHandlers)...)
    {
        static_assert(
            std::conjunction_v<std::disjunction<is_values_handler<Handlers>,
                                                is_categories_handler<Handlers>,
                                                is_any_handler<Handlers>>...>,
            "Every handler should belong to one of the types: values_handler, "
            "categories_handler or any_handler.");
    }

    template <typename R>
    constexpr int operator()(R &&aResult) const noexcept
    {
        using ResultT = utils::remove_cvref_t<R>;
        constexpr std::size_t num_error_types = ResultT::type_count - 1;
        using Indices = std::make_index_sequence<num_error_types>;
        return details::handlers_base<Handlers...>::process_impl(
            std::forward<R>(aResult), Indices{});
    }
};
}  // namespace tricky

#endif /* tricky_handlers_h */
