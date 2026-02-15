#ifndef UTILS_HPP
#define UTILS_HPP


#include <variant>


using namespace std;


// for inline constexpr auto XYZ = is_one<X, Y, Z>;
template <auto... Vals>
struct is_one_functor 
{
    constexpr bool operator()(auto x) const noexcept 
    {
        return ((x == Vals) || ...);
    }
};
template <auto... Vals>
inline constexpr auto is_one = is_one_functor<Vals...>{};




// for std::visit
template<class... Fs> struct overload : Fs... { using Fs::operator()...; };
template<class... Fs> overload(Fs...) -> overload<Fs...>;




// for better std::variant usage
template<typename T, class... Types>
inline bool operator==(const T& t, const variant<Types...>& v) 
{
    const T* c = get_if<T>(&v);
    return c && *c == t;
}
template<typename T, class... Types>
inline bool operator==(const variant<Types...>& v, const T& t) 
{
    return t == v;
}


#endif
