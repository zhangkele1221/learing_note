#ifndef RED_SEARCH_CPPCOMMON_REGISTRY_REGISTRY_INL_H_
#define RED_SEARCH_CPPCOMMON_REGISTRY_REGISTRY_INL_H_

#ifdef __RED_ENABLE_TR2__
#include <tr2/type_traits>
#endif

namespace red_search_cppcommon {
template <typename T, typename std::enable_if<T::empty::value, int>::type = 0>
void iterate_bases(std::vector<std::type_index> &, T *) {}

template <typename T, typename std::enable_if<!T::empty::value, int>::type = 0>
void iterate_bases(std::vector<std::type_index> &v, T *) {
    typedef typename T::first::type Base;
    v.push_back(std::type_index(typeid(Base)));
    typename T::rest::type * dummy = nullptr;
    iterate_bases(v, dummy);
}

template <typename T>
void TypeTraitsRegistry::register_type_traits(const char * name) {
    std::vector<std::type_index> base_types;
#ifdef __RED_ENABLE_TR2__
    iterate_bases<typename std::tr2::bases<T>::type>(base_types, nullptr);
#endif
    _mapping.emplace(std::string(name), std::type_index(typeid(T)));
    _base_type_mapping.emplace(std::string(name), base_types);
}
}  // namespace red_search_cppcommon

#endif  // RED_SEARCH_CPPCOMMON_REGISTRY_REGISTRY_INL_H_
