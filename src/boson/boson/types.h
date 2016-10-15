#ifndef BOSON_TYPES_H_
#define BOSON_TYPES_H_

#include "json_backbone.hpp"

namespace boson {
template <class ... T> using variant = json_backbone::variant<T ...>;
}

#endif  // BOSON_TYPES_H_
