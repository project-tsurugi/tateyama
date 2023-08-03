/*
 * appeared in https://stackoverflow.com/questions/18939882/raw-pointer-lookup-for-sets-of-unique-ptrs/18942830
 */
#pragma once

namespace tateyama::endpoint::common {

template<class T>
struct pointer_comp {
    using is_transparent = std::true_type;
    // helper does some magic in order to reduce the number of
    // pairs of types we need to know how to compare: it turns
    // everything into a pointer, and then uses `std::less<T*>`
    // to do the comparison:
    class helper {
        T* ptr;
    public:
        helper():ptr(nullptr) {}
        helper(helper const&) = default;
        helper(helper&&) noexcept = default;
        helper& operator = (helper const&) = default;
        helper& operator = (helper&&) noexcept = default;
        helper(T* const p):ptr(p) {}  //NOLINT
        template<class U>
        helper( std::shared_ptr<U> const& sp ):ptr(sp.get()) {}  //NOLINT
        template<class U, class...Ts>
        helper( std::unique_ptr<U, Ts...> const& up ):ptr(up.get()) {}  //NOLINT
        ~helper() = default;
        // && optional: enforces rvalue use only
        bool operator<( helper const o ) const {
            return std::less<T*>()( ptr, o.ptr );
        }
    };
    // without helper, we would need 2^n different overloads, where
    // n is the number of types we want to support (so, 8 with
    // raw pointers, unique pointers, and shared pointers).  That
    // seems silly:
    // && helps enforce rvalue use only
    bool operator()( helper const&& lhs, helper const&& rhs ) const {
        return lhs < rhs;
    }
};

} // namespace tateyama::endpoint::common
