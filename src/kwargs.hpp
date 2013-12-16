#ifndef KWARGS_HPP
#define KWARGS_HPP

// Ross Smith (rosssmith@me.com)
// This code is in the public domain

#include <type_traits>

namespace KwargsImpl {

    template <typename T, typename Val, bool = std::is_convertible<T, Val>::value>
    struct Ret {
        T operator()(const Val &v) const { return v; }
    };

    template <typename T, typename Val>
    struct Ret<T, Val, false> {
        T operator()(const Val &/*v*/) const { return T(); }
    };

    template <typename Val>
    struct Param {
        const void* key;
        Val value;
    };

}

template <typename Val>
struct Kwarg {
    Kwarg() {}
    template <typename Arg> KwargsImpl::Param<Val>
        operator=(const Arg& arg) const { return {this, arg}; }
};

template <typename Key, typename Val, typename Par, typename... Args>
Val kwget(const Kwarg<Key>& key, Val value,
        const KwargsImpl::Param<Par>& param, const Args&... more) {
    if (&key == param.key)
        return KwargsImpl::Ret<Val, decltype(param.value)>()(param.value);
    return kwget(key, value, more...);
}

template <typename Key, typename Val, typename... Args>
Val kwget(const Kwarg<Key>& key, Val value,
        const Kwarg<bool>& param, const Args&... more) {
    return kwget(key, value, param = true, more...);
}

template <typename Key, typename Val>
Val kwget(const Kwarg<Key>& /*k*/, const Val v) { return v; }

#endif
