#ifndef tricky_h
#define tricky_h

#include <user_literals/user_literals.h>

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

template <typename T, typename... Ts>
struct is_one_of
    : std::bool_constant<std::disjunction_v<std::is_same<T, Ts>...>>
{
};

template <typename T, typename... Ts>
inline constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

template <typename T, std::size_t kIndex, typename T0, typename... Ts>
struct type_index_impl;

template <typename T, std::size_t kIndex, bool kIsSameAsT0, typename T0,
          typename... Ts>
struct type_index_impl_impl : std::integral_constant<std::size_t, kIndex>
{
};

template <typename T, std::size_t kIndex, typename T0, typename... Ts>
struct type_index_impl_impl<T, kIndex, false, T0, Ts...>
    : type_index_impl<T, kIndex + 1_uz, Ts...>
{
};

template <typename T, std::size_t kIndex, typename T0, typename... Ts>
struct type_index_impl
    : type_index_impl_impl<T, kIndex, std::is_same_v<T, T0>, T0, Ts...>
{
};

template <typename T, typename T0, typename... Ts>
struct type_index : type_index_impl<T, 0_uz, T0, Ts...>
{
};

template <typename T, typename T0, typename... Ts>
inline constexpr std::size_t type_index_v = type_index<T, T0, Ts...>::value;

template <typename E, typename... Es>
union any_error
{
    E value;
    any_error<Es...> rest_values;

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

    inline constexpr any_error() noexcept : value() {}

    inline constexpr any_error(const any_error &) = default;
    inline constexpr any_error &operator=(const any_error &) noexcept = default;

    inline constexpr any_error &operator=(any_error &&) noexcept = default;
    inline constexpr any_error(any_error &&) noexcept = default;

    inline constexpr any_error(E aValue) noexcept : value(aValue) {}
};
}  // namespace details

template <typename T, typename Error, typename... Errors>
class result
{
   private:
    template <typename U, typename E, typename... Es>
    friend class result;

    template <typename U>
    static constexpr std::size_t type_index_v =
        details::type_index_v<U, T, Error, Errors...>;

    using stored_type = typename details::stored<T>::type;
    using value_type = typename details::stored<T>::value_type;
    using value_type_const = typename details::stored<T>::value_type_const;
    using value_ref = typename details::stored<T>::value_ref;
    using value_cref = typename details::stored<T>::value_cref;
    using value_rv_ref = typename details::stored<T>::value_rv_ref;
    using value_rv_cref = typename details::stored<T>::value_rv_cref;

   public:
    ~result() = default;
    inline constexpr result() noexcept : result(T{}) {}

    inline constexpr result(const result &) = default;
    inline constexpr result &operator=(const result &) noexcept = default;

    inline constexpr result &operator=(result &&) noexcept = default;
    inline constexpr result(result &&) noexcept = default;

    inline constexpr result(T aValue) noexcept
        : value_(aValue), type_index_(type_index_v<T>)
    {
    }

    template <typename E>
    inline constexpr result(E aError) noexcept
        : error_(aError), type_index_(type_index_v<E>)
    {
    }

    explicit constexpr operator bool() const noexcept { return has_value(); }

    inline constexpr value_cref value() const &
    {
        enforce_value_state();
        return value_;
    }

    inline constexpr value_ref value() &
    {
        enforce_value_state();
        return value_;
    }

    inline constexpr value_rv_cref value() const &&
    {
        enforce_value_state();
        return std::move(value_);
    }

    inline constexpr value_rv_ref value() &&
    {
        enforce_value_state();
        return std::move(value_);
    }

    inline constexpr Error error() const
    {
        enforce_error_state();
        return error_.value;
    }

    inline constexpr bool has_error() const noexcept { return type_index_; }

    inline constexpr bool has_value() const noexcept { return !has_error(); }

   private:
    inline constexpr void enforce_error_state() const
    {
        assert(has_error() && "this result must be in error state.");
    }

    inline constexpr void enforce_value_state() const
    {
        assert(has_value() && "this result must be in value state.");
    }

    union
    {
        stored_type value_;
        details::any_error<Error, Errors...> error_;
    };
    std::size_t type_index_{0_uz};
};

template <typename Error, typename... Errors>
class result<void, Error, Errors...>
    : public result<details::void_, Error, Errors...>
{
    template <typename U, typename E, typename... Es>
    friend class result;

    using void_ = details::void_;
    using base = result<void_, Error, Errors...>;

   public:
    using base::operator=;
    using base::operator bool;
    using value_type = void;

    ~result() = default;
    inline constexpr result() noexcept = default;
    inline constexpr result(const result &) noexcept = default;
    inline constexpr result(result &&) noexcept = default;

    template <typename E>
    inline constexpr result(E aError) noexcept : base(aError)
    {
    }

    inline constexpr void value() const { base::enforce_value_state(); }
};
}  // namespace tricky

#endif /* tricky_h */
