#ifndef UTILS_HPP
#define UTILS_HPP


template <auto... Vals>
struct is_one_functor {
    constexpr bool operator()(auto x) const noexcept 
    {
        return ((x == Vals) || ...);
    }
};
template <auto... Vals>
inline constexpr auto is_one = is_one_functor<Vals...>{};


#endif
