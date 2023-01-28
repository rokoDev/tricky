#ifndef tricky_payload_h
#define tricky_payload_h

#include <type_name/type_name.h>
#include <utils/utils.h>

#include <functional>
#include <new>

#include "data.h"

namespace tricky
{
template <typename T>
struct is_functor
{
    template <typename U>
    static std::true_type test(decltype(&U::operator()));

    template <typename U>
    static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
inline constexpr bool is_functor_v = is_functor<T>::value;

template <typename>
struct is_regular_function : std::false_type
{
};

template <typename R, typename... Args>
struct is_regular_function<R(Args...) noexcept> : std::true_type
{
    using return_type = R;
    using input_types = utils::type_list<Args...>;
};

template <typename T>
inline constexpr bool is_regular_function_v = is_regular_function<T>::value;

template <typename T>
struct is_invocable
    : std::bool_constant<
          std::disjunction_v<is_functor<T>, is_regular_function<T>>>
{
};

template <typename T>
inline constexpr bool is_invocable_v = is_invocable<T>::value;

template <typename T>
struct function_remove_const;

template <typename R, typename... Args>
struct function_remove_const<R(Args...)>
{
    using type = R(Args...);
};

template <typename R, typename... Args>
struct function_remove_const<R(Args...) const>
{
    using type = R(Args...);
};

template <typename T>
using function_remove_const_t = typename function_remove_const<T>::type;

template <typename T, typename U = std::enable_if_t<is_invocable_v<T>>>
struct callable_info
{
   private:
    template <typename>
    struct signature_getter
    {
    };

    template <class C, class M>
    struct signature_getter<M C::*>
    {
        using type = M;
    };

    template <typename R, typename... Args>
    struct signature_getter<R(Args...) noexcept>
    {
        using type = R(Args...);
    };

    template <typename C>
    static signature_getter<decltype(&C::operator())> test(
        decltype(&C::operator()));

    template <typename C>
    static signature_getter<C> test(...);

    template <typename C>
    struct function_signature;

    template <typename R, typename... Args>
    struct function_signature<R(Args...)>
    {
        using return_type = R;
        using params_type = utils::type_list<Args...>;
    };

   public:
    using signature = typename decltype(test<T>(0))::type;
    using return_type = typename function_signature<
        function_remove_const_t<signature>>::return_type;
    using params_type = typename function_signature<
        function_remove_const_t<signature>>::params_type;
};

template <std::size_t MaxSpace, std::size_t MaxCount>
class payload
{
   public:
    template <typename T>
    struct is_valid_argument_type
        : std::bool_constant<not std::is_reference_v<std::remove_const_t<T>> &&
                             not std::is_pointer_v<std::remove_const_t<T>> &&
                             not std::is_volatile_v<std::remove_const_t<T>>>
    {
    };

    template <typename T>
    struct is_valid_argument_type<T &> : std::bool_constant<std::is_const_v<T>>
    {
    };

    template <typename T>
    struct is_valid_argument_type<T *> : std::bool_constant<std::is_const_v<T>>
    {
    };

    template <typename T>
    struct is_valid_argument_type<T *const>
        : std::bool_constant<std::is_const_v<T>>
    {
    };

    template <typename T>
    static constexpr bool is_valid_argument_type_v =
        is_valid_argument_type<T>::value;

    static constexpr std::size_t kMaxSpace{MaxSpace};
    static constexpr std::size_t kMaxCount{MaxCount};

    ~payload() { reset(); }

    template <typename T>
    void load(T &&aArg) noexcept
    {
        if (count_ == kMaxCount)
        {
            return;
        }

        using CoreT = utils::remove_cvref_t<T>;
        constexpr auto kTypeName = type_name::type_name<CoreT>();
        auto add_value = [this, &kTypeName](auto &aCopyMaker,
                                            Destructor aDestructor,
                                            std::size_t aSize)
        {
            if (space_used_ + aSize <= kMaxSpace)
            {
                add_value_info(kTypeName, aCopyMaker(), aDestructor, aSize);
            }
            else
            {
                space_shortage_ = space_used_ + aSize - kMaxSpace;
            }
        };

        if constexpr (std::is_same_v<CoreT, c_str> || is_sequence_v<CoreT>)
        {
            auto add_value_sequence =
                [this, &kTypeName](auto &aSequenceCopyMaker, auto &aSequence)
            {
                if (aSequence.data())
                {
                    aSequenceCopyMaker();
                }
                else
                {
                    add_value_info(kTypeName, nullptr, nullptr, 0);
                }
            };
            if constexpr (std::is_same_v<CoreT, c_str>)
            {
                auto seq_copy_maker = [this, &aArg, &add_value]()
                {
                    const std::size_t size = std::strlen(aArg.data()) + 1;

                    auto make_copy = [this, src = aArg.data(),
                                      &size]() -> char *
                    {
                        char *value_ptr = data_ + space_used_;

                        // store value
                        std::memcpy(value_ptr, src, size);

                        return value_ptr;
                    };

                    add_value(make_copy, nullptr, size);
                };
                add_value_sequence(seq_copy_maker, aArg);
            }
            else
            {
                auto seq_copy_maker = [this, &aArg, &add_value]()
                {
                    using data_t = typename CoreT::type;
                    using count_t = typename CoreT::count_type;

                    static_assert(std::is_nothrow_copy_constructible_v<data_t>);
                    static_assert(std::is_nothrow_destructible_v<data_t>);

                    const auto count = aArg.count();
                    const auto count_skip =
                        skip_to_align<count_t>(data_ + space_used_);
                    constexpr auto count_size = sizeof(count_t);

                    const auto data_skip = skip_to_align<data_t>(
                        data_ + space_used_ + count_skip + count_size);
                    const auto data_size = count * sizeof(data_t);

                    const std::size_t size =
                        count_skip + count_size + data_skip + data_size;

                    auto make_copy = [this, &count, &count_skip, &data_skip,
                                      src = aArg.data()]() -> char *
                    {
                        char *value_ptr = data_ + space_used_ + count_skip;

                        // store count
                        new (value_ptr) count_t(count);

                        auto dst = value_ptr + sizeof(count_t) + data_skip;

                        // store data sequence
                        for (std::size_t i = 0; i < count;
                             ++i, dst += sizeof(data_t))
                        {
                            new (dst) data_t(*(src + i));
                        }
                        return value_ptr;
                    };

                    add_value(make_copy, (&payload::destruct<CoreT>), size);
                };
                add_value_sequence(seq_copy_maker, aArg);
            }
        }
        else
        {
            static_assert(std::is_nothrow_copy_constructible_v<CoreT>);
            static_assert(std::is_nothrow_destructible_v<CoreT>);
            const auto skip = skip_to_align<CoreT>(data_ + space_used_);
            const auto size = skip + sizeof(CoreT);

            auto make_copy = [this, &aArg, &skip]() -> char *
            {
                auto value_ptr = data_ + space_used_ + skip;

                // store value
                new (value_ptr) CoreT(aArg);

                return value_ptr;
            };

            add_value(make_copy, (&payload::destruct<CoreT>), size);
        }
    }

    template <typename... Callbacks>
    std::enable_if_t<not utils::is_tuple_v<utils::remove_cvref_t<Callbacks>...>,
                     bool>
    process(Callbacks &&...aCallbacks) const noexcept
    {
        return (... || call_if_match(std::forward<Callbacks>(aCallbacks)));
    }

    template <typename Callbacks>
    std::enable_if_t<utils::is_tuple_v<utils::remove_cvref_t<Callbacks>>, bool>
    process(Callbacks &&aCallbacks) const noexcept
    {
        using Indices = std::make_index_sequence<
            std::tuple_size_v<utils::remove_cvref_t<Callbacks>>>;
        return process_impl(std::forward<Callbacks>(aCallbacks), Indices{});
    }

    template <typename T>
    decltype(auto) extract(std::size_t aIndex) const noexcept
    {
        assert((aIndex < count_) && "invalid aIndex");
        const auto &info = values_[aIndex];
        assert((info.value_type == type_name::type_name<T>()) &&
               "information about type T was not found at index aIndex");
        if constexpr (std::is_same_v<T, c_str>)
        {
            return c_str(info.value_ptr);
        }
        else if constexpr (is_sequence_v<T>)
        {
            using count_t = typename T::count_type;
            using data_t = typename T::type;

            if (auto value_ptr = info.value_ptr; value_ptr)
            {
                const auto count =
                    *reinterpret_cast<count_t const *>(value_ptr);

                const auto data_skip =
                    skip_to_align<data_t>(value_ptr + sizeof(count_t));
                const auto data_ptr = reinterpret_cast<data_t const *>(
                    value_ptr + sizeof(count_t) + data_skip);

                return T(data_ptr, count);
            }
            else
            {
                return T(nullptr, count_t{});
            }
        }
        else
        {
            if constexpr (sizeof(T) <= sizeof(int))
            {
                return *reinterpret_cast<T const *>(info.value_ptr);
            }
            else
            {
                const T &val = *reinterpret_cast<T const *>(info.value_ptr);
                return val;
            }
        }
    }

    template <typename TList>
    std::enable_if_t<utils::is_type_list_v<TList>, bool> match() const noexcept
    {
        bool retVal{};
        validate_callback_arg_types<TList>();
        if constexpr (TList::size <= kMaxCount)
        {
            if (count_ >= TList::size)
            {
                using Indices = std::make_index_sequence<TList::size>;
                retVal = match_impl<TList>(Indices{});
            }
        }
        return retVal;
    }

    template <typename... Ts>
    std::enable_if_t<not utils::is_type_list_v<Ts...>, bool> match()
        const noexcept
    {
        return match<utils::type_list<Ts...>>();
    }

    static inline constexpr std::size_t max_space() noexcept
    {
        return kMaxSpace;
    }

    static inline constexpr std::size_t max_count() noexcept
    {
        return kMaxCount;
    }

    inline constexpr std::size_t count() const noexcept { return count_; }

    inline constexpr std::size_t space_used() const noexcept
    {
        return space_used_;
    }

    inline constexpr std::size_t space_shortage() const noexcept
    {
        return space_shortage_;
    }

    inline constexpr const auto &data() const noexcept { return data_; }

    constexpr void reset() noexcept
    {
        for (std::size_t i = 0; i < count_; ++i)
        {
            auto &info = values_[i];
            if (info.destructor)
            {
                std::invoke(info.destructor, *this, info.value_ptr);
                info.destructor = nullptr;
            }
            info.value_type = std::string_view{};
            info.value_ptr = nullptr;
        }

        count_ = 0;
        space_used_ = 0;
        space_shortage_ = 0;
    }

   private:
    using Destructor = void (payload::*)(char *) noexcept;
    template <typename T>
    void destruct(char *aTPtr) noexcept
    {
        assert(aTPtr &&
               "trying to destruct object that located at nullptr address");
        if constexpr (is_sequence_v<T>)
        {
            using data_t = typename T::type;
            using count_t = typename T::count_type;

            auto *count = reinterpret_cast<count_t *>(aTPtr);

            const auto data_skip =
                skip_to_align<data_t>(aTPtr + sizeof(count_t));
            auto data_ptr =
                reinterpret_cast<data_t *>(aTPtr + sizeof(count_t) + data_skip);

            for (std::size_t i = 0; i < *count; ++i, ++data_ptr)
            {
                data_ptr->~data_t();
            }
            count->~count_t();
        }
        else
        {
            auto obj = reinterpret_cast<T *>(aTPtr);
            obj->~T();
        }
    }

    template <typename TList>
    static constexpr std::enable_if_t<utils::is_type_list_v<TList>>
    validate_callback_arg_types() noexcept
    {
        constexpr auto kValidArgsCount =
            TList::template count_of_predicate_compliant<
                is_valid_argument_type>;
        static_assert(
            kValidArgsCount == TList::size,
            "It is must be impossible to change values stored in payload via "
            "callback' arguments. Therefore argument type should be const "
            "reference, pointer to const or copied by value. For details see "
            "is_valid_argument_type.");
    }

    template <typename Callbacks, std::size_t... I>
    bool process_impl(Callbacks &&aCallbacks,
                      std::index_sequence<I...>) const noexcept
    {
        return process(std::get<I>(aCallbacks)...);
    }

    template <typename Callback>
    bool call_if_match(Callback &&aCallback) const noexcept
    {
        using Invokable =
            std::remove_pointer_t<utils::remove_cvref_t<Callback>>;
        static_assert(is_invocable_v<Invokable>);
        using R = typename callable_info<Invokable>::return_type;
        static_assert(std::is_same_v<R, void>);
        using ParamsType = typename callable_info<Invokable>::params_type;
        bool is_called{};
        if (match<ParamsType>())
        {
            using Indices = std::make_index_sequence<ParamsType::size>;
            make_call<ParamsType>(std::forward<Callback>(aCallback), Indices{});
            is_called = true;
        }
        return is_called;
    }

    template <typename ArgTypes, typename Callback, std::size_t... I>
    void make_call(Callback &&aCallback,
                   std::index_sequence<I...>) const noexcept
    {
        aCallback(
            extract<utils::remove_cvref_t<typename ArgTypes::template at<I>>>(
                I)...);
    }

    void add_value_info(std::string_view aTypeName, char *aValuePtr,
                        Destructor aDestructor, std::size_t aSize) noexcept
    {
        // save info needed to extract stored type name and value
        values_[count_] = {aTypeName, aValuePtr, aDestructor};

        // increment count_ and space_used_
        ++count_;
        space_used_ += aSize;
    }

    template <typename T>
    static constexpr uintptr_t skip_to_align(void const *aPtr) noexcept
    {
        constexpr auto kAlignment = alignof(T);
        static_assert(utils::is_power_of_2(kAlignment));
        const uintptr_t ptrAsUInt = reinterpret_cast<uintptr_t>(aPtr);
        const uintptr_t alignedPtr =
            (ptrAsUInt + (kAlignment - 1)) & -kAlignment;
        return alignedPtr - ptrAsUInt;
    }

    template <typename TList, std::size_t... I>
    bool match_impl(std::index_sequence<I...>) const noexcept
    {
        return (
            ... &&
            (values_[I].value_type ==
             (type_name::type_name<
                 utils::remove_cvref_t<typename TList::template at<I>>>())));
    }

    struct value_info
    {
        std::string_view value_type{};
        char *value_ptr{};
        Destructor destructor{nullptr};
    };

    std::size_t count_{};
    std::size_t space_used_{};
    std::size_t space_shortage_{};

    value_info values_[kMaxCount]{};
    char data_[kMaxSpace]{};
};
}  // namespace tricky

#endif /* tricky_payload_h */
