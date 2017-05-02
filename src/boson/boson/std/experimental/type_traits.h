#ifndef BOSON_STD_EXPERIMENTAL_TYPE_TRAITS_H_
#define BOSON_STD_EXPERIMENTAL_TYPE_TRAITS_H_

#include <type_traits>

namespace boson {
namespace experimental {
template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;
}
}

#endif
