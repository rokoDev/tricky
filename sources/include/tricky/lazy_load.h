#ifndef tricky_lazy_load_h
#define tricky_lazy_load_h

#include <tuple>
#include <utility>

#include "state.h"

namespace tricky
{
template <typename... Ts>
class lazy_load
{
   public:
    lazy_load &operator=(const lazy_load &) = delete;

    lazy_load(lazy_load &&aOther) noexcept
        : cargo_{std::move(aOther.cargo_)}, moved_{false}
    {
        aOther.moved_ = true;
    }

    explicit lazy_load(Ts &&...aArg) noexcept
        : cargo_(std::forward<Ts>(aArg)...), moved_{false}
    {
    }

    ~lazy_load()
    {
        if (moved_)
        {
            return;
        }
        if (shared_state::has_value())
        {
            return;
        }
        using Indices = std::make_index_sequence<sizeof...(Ts)>;
        preload_each_tuple_item(std::move(cargo_), Indices{});
    }

   private:
    template <typename T, std::size_t... I>
    static void preload_each_tuple_item(T &&aTuple,
                                        std::index_sequence<I...>) noexcept
    {
        (..., shared_state::load(std::get<I>(std::forward<T>(aTuple))));
    }

    std::tuple<Ts...> cargo_;
    bool moved_{};
};

template <typename... Ts>
[[nodiscard]] inline decltype(auto) on_error(Ts &&...aForPayload) noexcept
{
    return lazy_load(std::forward<Ts>(aForPayload)...);
}
}  // namespace tricky

#endif /* tricky_lazy_load_h */
