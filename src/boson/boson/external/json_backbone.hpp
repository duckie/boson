#ifndef JSON_BACKBONE_BASE_HEADER
#define JSON_BACKBONE_BASE_HEADER
#include <type_traits>
#include <utility>
#include <functional>
#include <exception>
#include <limits>
#include <array>
#include <initializer_list>

namespace json_backbone {
//
// is_complete detects if type is complete at first instantiation
//
// "at first instantiation" is utterly important so that ODR would not be violated
// Unfortunetaly, clang behaves badly with it and produces completely
// erroneous code. Thus we rely on recursive_wrapper.
//
// This is not completely broken in Clang but Clang does not behave if this is
// instantiated from within the type
//
template <typename T>
struct is_complete {
  static T f(int);
  template <class U>
  static constexpr bool g(decltype((void)f(std::declval<U>()))*) {
    return true;
  }
  template <class U>
  static constexpr bool g(...) {
    return false;
  }
  static constexpr bool value = g<int>(0);
};

// Recursive helper - any incomplete type is considered recursive (broken on Clang)
template <class T>
struct recursive_wrapper {};  // Instantiated to make noexcept rules to compile with the tuple trick
template <class T>
struct is_recursive : std::integral_constant<bool, !is_complete<T>::value> {};
template <class T>
struct is_recursive<recursive_wrapper<T>> : std::true_type {};

template <class T>
using identity_t = T;

// bounded_identity represents the implementend type, it extracts
// the real type from its recursive property
template <class T>
struct bounded_identity {
  using type = T;
};
template <class T>
struct bounded_identity<recursive_wrapper<T>> {
  using type = T;
};
template <class T>
using bounded_identity_t = typename bounded_identity<T>::type;

// Is small type
template <class T>
struct is_small_type : std::integral_constant<bool, (sizeof(T) <= sizeof(T*))> {};
template <class T, bool IsRecursive = is_recursive<T>::value>
struct is_small_type_impl;
template <class T>
struct is_small_type_impl<T, false> : std::integral_constant<bool, is_small_type<T>::value> {};
template <class T>
struct is_small_type_impl<T, true> : std::false_type {};

//
// memory_footprint_t represents minimum size needed to store the type
//
// This is the minimum size to be allocated in a container to hold
// this type wether it is a small type or not. Big types needs to
// be pointed to while small ones are directly stored
//
template <class T, bool IsRecursive = is_recursive<T>::value>
struct memory_footprint;
template <class T>
struct memory_footprint<T, false>
    : std::integral_constant<std::size_t,
                             (is_small_type_impl<T>::value ? sizeof(T) : sizeof(void*))> {};
template <class T>
struct memory_footprint<T, true> : std::integral_constant<std::size_t, sizeof(void*)> {};

// Helper to generate adequate allocations and deallocation functions
template <class T, std::size_t MemSize>
struct store_on_stack : std::integral_constant<bool, (is_small_type_impl<T>::value &&
                                                      memory_footprint<T>::value <= MemSize)> {};

//
// arithmetics provides compile time arithmetics over arrays
//
namespace arithmetics {

template <class I, std::size_t N>
I constexpr max_value(std::array<I, N> const& values,
                      I current_value = std::numeric_limits<I>::lowest(),
                      std::size_t current_index = 0) {
  return N <= current_index ? current_value
                            : (current_value < values.at(current_index)
                                   ? max_value(values, values.at(current_index), current_index + 1)
                                   : max_value(values, current_value, current_index + 1));
}

template <class I, std::size_t N>
std::size_t constexpr find_first(std::array<I, N> const& values, I value,
                                 std::size_t current_index = 0) {
  return N <= current_index ? N : value == values.at(current_index)
                                      ? current_index
                                      : find_first(values, value, current_index + 1);
}

template <class I, std::size_t N>
std::size_t constexpr find_last(std::array<I, N> const& values, I value,
                                std::size_t current_index = 0) {
  return N <= current_index ? N : value == values.at(N - current_index - 1)
                                      ? (N - current_index - 1)
                                      : find_last(values, value, current_index + 1);
}

template <class I, std::size_t N>
bool constexpr all_equals(std::array<I, N> const& values, I current_value,
                          std::size_t current_index = 0) {
  return N <= current_index ? true : (current_value == values.at(current_index))
                                         ? all_equals(values, current_value, current_index + 1)
                                         : false;
}
};

//
// Utilities for computation on type lists
//
namespace type_list_traits {

// type_list forward declaration
template <class Indices, class... Types>
struct type_list;

// Utility type
template <class T>
struct type_holder {
  using type = T;
};

// Utility type
template <class Type, std::size_t Index>
struct type_info {};

//
// type_list offers compile time services on a type list
//
// type_list is implemented with function overloads rather than than recursive
// templates to speed up compilation.
//
template <std::size_t... Is, class... Types>
struct type_list<std::index_sequence<Is...>, Types...> : type_info<Types, Is>... {
  //
  // Returns the index as integral constant of the given type in the list
  //
  // Not supposed to be called, implementation is just there to avoid
  // warnings
  //
  template <class Type, std::size_t Index>
  static constexpr std::integral_constant<std::size_t, Index> type_index(
      type_info<Type, Index> const&) {
    return {};
  }

  //
  // Sink member function to return a value for unsupported types
  //
  // Not supposed to be called, implementation is just there to avoid
  // warnings
  //
  template <class Type>
  static constexpr std::integral_constant<std::size_t, sizeof...(Types)> type_index(...) {
    return {};
  }

  //
  // Returns the type at a given index
  //
  // Not supposed to be called, implementation is just there to avoid
  // warnings
  //
  template <std::size_t Index, class Type>
  static constexpr type_holder<Type> get_type_at(type_info<Type, Index> const&) {
    return {};
  }

  //
  // Void sink for unsupported types
  //
  // Not supposed to be called, implementation is just there to avoid
  // warnings
  //
  template <std::size_t Index>
  static constexpr type_holder<void> get_type_at(...) {
    return {};
  }

  //
  // Returns index of type T in the list
  //
  // Returns size of the list if T not in the list
  //
  template <class T>
  static constexpr std::size_t get_index() {
    return decltype(type_index<T>(std::declval<type_list>()))::value;
  }

  //
  // Returns the type at a given index
  //
  // Not supposed to be called, implementation is just there to avoid
  // warnings
  //
  template <std::size_t Index>
  struct type_at {
    using type_impl = decltype(get_type_at<Index>(std::declval<type_list>()));
    using type = typename type_impl::type;
  };

  // Returns true if type T is in the list
  template <class T>
  static constexpr bool has_type() {
    return decltype(type_index<T>(std::declval<type_list>()))::value < sizeof...(Types);
  }

  //
  // Returns a type chosen for construction following those rules
  //
  // All types must be complete at time of initialization.
  // If there is a single argument and it is equal to one of the bounded types, this type is
  // selected
  // If there is a single argument and it is a reference (either l-value or r-value) to an integral
  // type, the first integral type of the list wide enough and with the same signature is used to
  // store the value.
  // If there is a single argument and it is a reference (either l-value or r-value) to an integral
  // type, the first integral type of the list wide enough and with any signature is used to store
  // the value.
  // If there is a single argument and it is a reference (either l-value or r-value) to an
  // arithmetic type, the first arityhmetic but not integral type of the list wide enough is used to
  // store the value.
  // If there is a single argument and it is a pointer or an array , the first non-integral type
  // supporting construction over this pointer is used to store the value.
  // If none of those rules applies, the first type encountered supporting the signature is used to
  // store the value.
  //
  // Constructibility covers convertibility, so is used for the sake
  // of implicit conversion too
  //
  template <std::size_t MemSize, class Arg, class... Args>
  struct select_constructible {
    static constexpr std::size_t index_first_same_value =
        arithmetics::find_first<bool, sizeof...(Types)>(
            {(std::is_same<std::decay_t<Arg>, Types>() &&
              std::is_constructible<Types, Arg, Args...>::value)...},
            true);
    static constexpr std::size_t index_first_integral_ssig_value =
        arithmetics::find_first<bool, sizeof...(Types)>(
            {(std::is_integral<Types>() && sizeof(Arg) <= memory_footprint<Types>::value &&
              std::is_signed<Types>() == std::is_signed<Arg>() &&
              std::is_constructible<Types, Arg, Args...>::value)...},
            true);
    static constexpr std::size_t index_first_integral_value =
        arithmetics::find_first<bool, sizeof...(Types)>(
            {(std::is_integral<Types>() && sizeof(Arg) <= memory_footprint<Types>::value &&
              std::is_constructible<Types, Arg, Args...>::value)...},
            true);
    static constexpr std::size_t index_first_arithmetic_value =
        arithmetics::find_first<bool, sizeof...(Types)>(
            {(std::is_arithmetic<Types>() && !std::is_integral<Types>() &&
              sizeof(Arg) <= memory_footprint<Types>::value &&
              std::is_constructible<Types, Arg, Args...>::value)...},
            true);
    static constexpr std::size_t index_first_ptrwise_value =
        arithmetics::find_first<bool, sizeof...(Types)>(
            {(!(std::is_integral<Types>() && std::is_pointer<std::decay_t<Arg>>() &&
                0 == sizeof...(Args)) &&
              std::is_constructible<Types, Arg, Args...>::value)...},
            true);
    static constexpr std::size_t index_first_value =
        arithmetics::find_first<bool, sizeof...(Types)>(
            {(std::is_constructible<Types, Arg, Args...>::value)...}, true);

    static constexpr std::size_t index_value =
        (index_first_same_value < sizeof...(Types) && 0 == sizeof...(Args))
            ? index_first_same_value
            : (std::is_integral<Arg>() && 0 == sizeof...(Args) &&
               index_first_integral_ssig_value < sizeof...(Types))
                  ? index_first_integral_ssig_value
                  : (std::is_integral<Arg>() && 0 == sizeof...(Args) &&
                     index_first_integral_value < sizeof...(Types))
                        ? index_first_integral_value
                        : (std::is_arithmetic<Arg>() && 0 == sizeof...(Args) &&
                           index_first_arithmetic_value < sizeof...(Types))
                              ? index_first_arithmetic_value
                              : (index_first_ptrwise_value < sizeof...(Types))
                                    ? index_first_ptrwise_value
                                    : index_first_value;
    using type_impl = decltype(get_type_at<index_value>(std::declval<type_list>()));
    using type = typename type_impl::type;
  };
};

//
// Bounded types traits
//
// Those traits are supposed to work even with incomplete types
//
template <class... Types>
struct bounded_type_traits {
  template <class T, bool IsRecursive>
  struct is_default_constructible_impl;
  template <class T>
  struct is_default_constructible_impl<T, true> : std::false_type {};
  template <class T>
  struct is_default_constructible_impl<T, false>
      : std::integral_constant<bool, std::is_default_constructible<bounded_identity_t<T>>::value> {
  };

  template <class T>
  struct is_default_constructible : is_default_constructible_impl<T, is_recursive<T>::value> {};

  //
  // Returns a type chosen for default construction
  //
  // Rules:
  // - Select first default constructible type
  //
  struct select_default {
    static constexpr std::size_t index_value = arithmetics::find_first<bool, sizeof...(Types)>(
        {is_default_constructible<Types>::value...}, true);
  };
};
}  // namespace type_list_traits

//
// helpers creates functions to be used by the variant
//
namespace helpers {

// deleter_fp is a function that deletes a type - small type version
template <class T, class Impl, std::size_t MemSize>
std::enable_if_t<store_on_stack<T, MemSize>::value, void> deleter_fp(void** data) {
  reinterpret_cast<Impl*>(data)->~Impl();
}

// deleter_fp is a function that deletes a type - big type version
template <class T, class Impl, std::size_t MemSize>
std::enable_if_t<!store_on_stack<T, MemSize>::value, void> deleter_fp(void** data) {
  delete reinterpret_cast<Impl*>(*data);
}

}  // namespace helpers

// bad_variant_access is thrown at runtime when accessing a container with the wrong type
struct bad_variant_access : std::logic_error {
  using std::logic_error::logic_error;
};

// bad_container_object_access is thrown at runtime when accessing a nin-object container
// with an object API, or if a requested element does not exist and
// cannot be created
struct bad_container_object_access : std::logic_error {
  using std::logic_error::logic_error;
};

// bad_container_array_access is thrown at runtime when accessing a non-array container
// with an bound checking ("at") array API. No-check API ("operator[]") does not throw
struct bad_container_array_access : public std::logic_error {
  using std::logic_error::logic_error;
};

// Forward declare friends

// Template helper for auto-detection of recrusive types. Broken on Clang
template <class T>
struct completeness_test {
  static constexpr bool value = T::test_complete;
};

//
// variant is a discriminated union optimized for small types
//
template <class... Value>
class variant {
  // Helpers for auto-detection of recrusive types. Broken on Clang
  static constexpr bool test_complete = arithmetics::all_equals<bool, sizeof...(Value)>(
      {(is_complete<Value>::value || true)...}, true);
  friend struct completeness_test<variant>;

  // Compute minimum size required by types. Default 8
  static constexpr std::size_t min_memory_size =
      arithmetics::max_value<std::size_t, sizeof...(Value)>({memory_footprint<Value>::value...});

 public:
  // Compute memory size of an array of void* wide enough to hold min_memory_size
  static constexpr std::size_t memory_size =
      sizeof(void*) * (min_memory_size / sizeof(void*) + (min_memory_size % sizeof(void*) ? 1 : 0));

 private:
  std::size_t type_ = sizeof...(Value);
  void* data_[memory_size / sizeof(void*)] = {};

 public:
  // Original list of types kept to know wether a type is recursive or not
  using type_list_t =
      type_list_traits::type_list<std::make_index_sequence<sizeof...(Value)>, Value...>;
  // List of implemented types used to find constructors
  using target_type_list_t = type_list_traits::type_list<std::make_index_sequence<sizeof...(Value)>,
                                                         bounded_identity_t<Value>...>;

  // Limited type list used for incomplete contexts only
  using bounded_traits_t = type_list_traits::bounded_type_traits<Value...>;

 private:
  // Fails to compile if type is not supported
  template <class T>
  static inline constexpr void assert_has_type() {
    static_assert(target_type_list_t::template has_type<bounded_identity_t<std::decay_t<T>>>(),
                  "Type T not supported by this container.");
  }

  template <class T>
  struct resolve_type {
    using target_type = std::decay_t<T>;
    using type_index =
        std::integral_constant<std::size_t,
                               target_type_list_t::template get_index<std::decay_t<target_type>>()>;
    using type = typename type_list_t::template type_at<type_index::value>::type;
    using on_stack_type = store_on_stack<type, memory_size>;
  };

  using default_type = typename target_type_list_t::template type_at<
      bounded_traits_t::select_default::index_value>::type;

  template <class T, class Return>
  using enable_if_stack_t = std::enable_if_t<resolve_type<T>::on_stack_type::value, Return>;
  template <class T, class Return>
  using enable_if_heap_t = std::enable_if_t<!resolve_type<T>::on_stack_type::value, Return>;

  //
  // Destroys currently held object and deallocates heap if needed
  //
  void clear() {
    static std::array<void (*)(void**), sizeof...(Value)> deleters = {
        helpers::deleter_fp<Value, bounded_identity_t<Value>, memory_size>...};
    if (type_ < sizeof...(Value))  // Should always be the case
      deleters[type_](&data_[0]);
  }

  template <class T, class Arg, class... Args>
  enable_if_stack_t<T, void> allocate(Arg&& arg, Args&&... args) {
    new (reinterpret_cast<T*>(&data_[0])) T(std::forward<Arg>(arg), std::forward<Args>(args)...);
  }

  template <class T, class Arg, class... Args>
  enable_if_heap_t<T, void> allocate(Arg&& arg, Args&&... args) {
    T** ptr = reinterpret_cast<T**>(&data_[0]);
    *ptr = new T(std::forward<Arg>(arg), std::forward<Args>(args)...);
  }

  template <class Arg, class... Args>
  void create(Arg&& arg, Args&&... args) {
    using target_type =
        typename target_type_list_t::template select_constructible<memory_size, Arg, Args...>::type;
    assert_has_type<target_type>();
    using resolved_type = resolve_type<target_type>;
    type_ = resolved_type::type_index::value;
    allocate<target_type>(std::forward<Arg>(arg), std::forward<Args>(args)...);
  }

  template <class T>
  static void copy_ctor_fp(variant& self, variant const& other) {
    self.create(other.template raw<bounded_identity_t<T>>());
  }

  template <class T>
  static void move_ctor_fp(variant& self, variant&& other) {
    self.create(std::move(other.template raw<bounded_identity_t<T>>()));
  }

  template <class T>
  static void copy_assign_fp(variant& self, variant const& other) {
    self.template get<bounded_identity_t<T>>() = other.template raw<bounded_identity_t<T>>();
  }

  template <class T>
  static void move_assign_fp(variant& self, variant&& other) {
    self.template get<bounded_identity_t<T>>() =
        std::move(other.template raw<bounded_identity_t<T>>());
  }

 public:
  // Template resolution must work event with incomplete types here
  template <bool HasDefault = (bounded_traits_t::select_default::index_value < sizeof...(Value)),
            class Enabler = std::enable_if_t<HasDefault, void>>
  variant() noexcept(std::is_nothrow_default_constructible<default_type>::value) {
    create(default_type());
  }

  variant(variant const& other) noexcept(arithmetics::all_equals<bool, sizeof...(Value)>(
      {std::is_nothrow_copy_constructible<Value>::value...}, true)) {
    static std::array<void (*)(variant&, variant const&), sizeof...(Value)> ctors = {
        copy_ctor_fp<Value>...};
    ctors[other.type_](*this, other);
  }

  variant(variant&& other) noexcept(arithmetics::all_equals<bool, sizeof...(Value)>(
      {std::is_nothrow_move_constructible<Value>::value...}, true)) {
    static std::array<void (*)(variant&, variant&&), sizeof...(Value)> ctors = {
        move_ctor_fp<Value>...};
    ctors[other.type_](*this, std::move(other));
  }

  // Construction with a compatible constructor from a bounded type
  template <
      class Arg, class... Args,
      class Enabler = std::enable_if_t<
          !(std::is_base_of<variant, std::decay_t<Arg>>::value && 0 == sizeof...(Args)), void>>
  variant(Arg&& arg, Args&&... args) noexcept(
      std::is_nothrow_constructible<typename target_type_list_t::template select_constructible<
                                        memory_size, Arg, Args...>::type,
                                    Arg, Args...>::value) {
    static_assert(
        target_type_list_t::template select_constructible<memory_size, Arg, Args...>::index_value <
            sizeof...(Value),
        "Construction not supported by the variant.");
    using target_type =
        typename target_type_list_t::template select_constructible<memory_size, Arg, Args...>::type;
    assert_has_type<target_type>();
    create(std::forward<Arg>(arg), std::forward<Args>(args)...);
  }

  // Non-virtual destructor to spare a useless virtual table
  ~variant() { clear(); }

  // Assign from other variant
  variant& operator=(variant const& other) noexcept(arithmetics::all_equals<bool, sizeof...(Value)>(
      {std::is_nothrow_copy_assignable<Value>::value...}, true)) {
    static std::array<void (*)(variant&, variant const&), sizeof...(Value)> stores = {
        copy_assign_fp<Value>...};

    static std::array<void (*)(variant&, variant const&), sizeof...(Value)> ctors = {
        copy_ctor_fp<Value>...};

    if (type_ == other.type_) {
      stores[type_](*this, other);
    } else {
      clear();
      ctors[other.type_](*this, other);
    }
    return *this;
  }

  variant& operator=(variant&& other) noexcept(arithmetics::all_equals<bool, sizeof...(Value)>(
      {std::is_nothrow_move_assignable<Value>::value...}, true)) {
    static std::array<void (*)(variant&, variant&&), sizeof...(Value)> stores = {
        move_assign_fp<Value>...};

    static std::array<void (*)(variant&, variant&&), sizeof...(Value)> ctors = {
        move_ctor_fp<Value>...};

    if (type_ == other.type_) {
      stores[type_](*this, std::move(other));
    } else {
      clear();
      ctors[other.type_](*this, std::move(other));
    }
    return *this;
  }

  inline size_t type_index() const { return type_; }

  template <class T>
  static constexpr size_t type_index() {
    assert_has_type<std::decay_t<T>>();
    return target_type_list_t::template get_index<std::decay_t<T>>();
  }

  // Assignation from a bounded type
  template <class T, class Enabler = std::enable_if_t<
                         target_type_list_t::template has_type<std::decay_t<T>>(), void>>
  variant& assign(T&& value) {
    assert_has_type<std::decay_t<T>>();
    if (type_ == target_type_list_t::template get_index<std::decay_t<T>>()) {
      raw<std::decay_t<T>>() = std::forward<T>(value);
    } else {
      clear();
      this->create(std::forward<T>(value));
    }
    return *this;
  }

  // Assignation from a type convertible to one of the bounded types
  template <class T, class Enabler = std::enable_if_t<
                         !(target_type_list_t::template has_type<std::decay_t<T>>() ||
                           std::is_base_of<variant, std::decay_t<T>>::value),
                         void>,
            class Shim1 = void>
  variant& assign(T&& value) {
    static_assert(target_type_list_t::template select_constructible<memory_size, T>::index_value <
                      sizeof...(Value),
                  "Assignation not supported by the variant.");
    return assign(typename target_type_list_t::template select_constructible<memory_size, T>::type{
        std::forward<T>(value)});
  }

  // Assignation operators dispatches call on the matching assign.
  template <class T, class Enabler =
                         std::enable_if_t<!std::is_base_of<variant, std::decay_t<T>>::value, void>>
  inline variant& operator=(T&& value) {
    return assign(std::forward<T>(value));
  }

  template <class T>
  inline bool is() const noexcept {
    assert_has_type<T>();
    return type_ == target_type_list_t::template get_index<T>();
  }

  // get checks the type is correct and returns it
  template <class T>
  enable_if_stack_t<T, T&> get() & {
    assert_has_type<T>();
    if (is<T>()) return *(reinterpret_cast<T*>(&data_[0]));
    throw bad_variant_access{"Bad variant type in get."};
  }

  // get checks the type is correct and returns it
  template <class T>
  enable_if_heap_t<T, T&> get() & {
    assert_has_type<T>();
    if (is<T>()) return *(reinterpret_cast<T*>(data_[0]));
    throw bad_variant_access{"Bad variant type in get."};
  }

  // get checks the type is correct and returns it
  template <class T>
  enable_if_stack_t<T, T const&> get() const & {
    assert_has_type<T>();
    if (is<T>()) return *(reinterpret_cast<T const*>(&data_[0]));
    throw bad_variant_access{"Bad variant type in get."};
  }

  // get checks the type is correct and returns it
  template <class T>
  enable_if_heap_t<T, T const&> get() const & {
    assert_has_type<T>();
    if (is<T>()) return *(reinterpret_cast<T const*>(data_[0]));
    throw bad_variant_access{"Bad variant type in get."};
  }

  // get checks the type is correct and returns it
  template <class T>
  enable_if_stack_t<T, T> get() && {
    assert_has_type<T>();
    if (is<T>()) return std::move(*(reinterpret_cast<T*>(&data_[0])));
    throw bad_variant_access{"Bad variant type in get."};
  }

  // get checks the type is correct and returns it
  template <class T>
  enable_if_heap_t<T, T> get() && {
    assert_has_type<T>();
    if (is<T>()) return std::move(*(reinterpret_cast<T*>(data_[0])));
    throw bad_variant_access{"Bad variant type in get."};
  }

  // raw returns directly without any runtime check
  template <class T>
      inline enable_if_stack_t<T, T&> raw() & noexcept {
    assert_has_type<T>();
    return *(reinterpret_cast<T*>(&data_[0]));
  }

  // raw returns directly without any runtime check
  template <class T>
      inline enable_if_heap_t<T, T&> raw() & noexcept {
    assert_has_type<T>();
    return *(reinterpret_cast<T*>(data_[0]));
  }

  // raw returns directly without any runtime check
  template <class T>
  inline enable_if_stack_t<T, T const&> raw() const& noexcept {
    assert_has_type<T>();
    return *(reinterpret_cast<T const*>(&data_[0]));
  }

  // raw returns directly without any runtime check
  template <class T>
  inline enable_if_heap_t<T, T const&> raw() const& noexcept {
    assert_has_type<T>();
    return *(reinterpret_cast<T const*>(data_[0]));
  }

  // raw returns directly without any runtime check
  template <class T>
      inline enable_if_stack_t<T, T> raw() && noexcept {
    assert_has_type<T>();
    return std::move(*(reinterpret_cast<T*>(&data_[0])));
  }

  // raw returns directly without any runtime check
  template <class T>
      inline enable_if_heap_t<T, T> raw() && noexcept {
    assert_has_type<T>();
    return std::move(*(reinterpret_cast<T*>(data_[0])));
  }

  // Conversion operator
  template <class T, class Enabler = std::enable_if_t<
                         target_type_list_t::template has_type<std::decay_t<T>>(), void>>
  explicit operator T&() & {
    return this->get<std::decay_t<T>>();
  }

  template <class T, class Enabler = std::enable_if_t<
                         target_type_list_t::template has_type<std::decay_t<T>>(), void>>
  explicit operator T const&() const & {
    return this->get<std::decay_t<T>>();
  }

  template <class T, class Enabler = std::enable_if_t<
                         target_type_list_t::template has_type<std::decay_t<T>>(), void>>
  explicit operator T() && {
    return std::move(this->get<std::decay_t<T>>());
  }
};

// Functional version of is
template <class T, class... Value>
bool is(variant<Value...> const& value) {
  return value.template is<T>();
}

// Functional version of get
template <class T, class... Value>
T get(variant<Value...>& value) {
  return value.template get<T>();
}

// Functional version of get
template <class T, class... Value>
T const& get(variant<Value...> const& value) {
  return value.template get<T>();
}

// Functional version of get
template <class T, class... Value>
T get(variant<Value...>&& value) {
  return std::move(value.template get<T>());
}

// Functional version of raw
template <class T, class... Value>
inline decltype(auto) raw(variant<Value...>& value) noexcept {
  return value.template raw<T>();
}

// Functional version of raw
template <class T, class... Value>
inline decltype(auto) raw(variant<Value...> const& value) noexcept {
  return value.template raw<T>();
}

// Functional version of raw
template <class T, class... Value>
inline decltype(auto) raw(variant<Value...>&& value) noexcept {
  return std::move(value.template raw<T>());
}

//
// container is a variant extended with an associative container and a random access container
//
template <template <class...> class ObjectBase, template <class...> class ArrayBase, class Key,
          class... Value>
class container
    : public variant<Value..., ArrayBase<container<ObjectBase, ArrayBase, Key, Value...>>,
                     ObjectBase<Key, container<ObjectBase, ArrayBase, Key, Value...>>> {
 public:
  using variant_type = variant<Value..., ArrayBase<container<ObjectBase, ArrayBase, Key, Value...>>,
                               ObjectBase<Key, container<ObjectBase, ArrayBase, Key, Value...>>>;
  using object_type = ObjectBase<Key, container>;
  using array_type = ArrayBase<container>;
  using key_type = Key;
  using value_type_list_type =
      type_list_traits::type_list<std::make_index_sequence<sizeof...(Value)>, Value...>;

  container() : variant_type{} {}

  container(container const& value) : variant_type(value) {}

  container(container&& value) : variant_type(std::move(value)) {}

  template <class Arg, class... Args>
  container(Arg&& arg, Args&&... args)
      : variant_type(std::forward<Arg>(arg), std::forward<Args>(args)...) {}

  container& operator=(container const& value) {
    variant_type::operator=(value);
    return *this;
  }

  container& operator=(container&& value) {
    variant_type::operator=(std::move(value));
    return *this;
  }

  template <class Arg>
  container& operator=(Arg&& value) {
    variant_type::operator=(std::forward<Arg>(value));
    return *this;
  }

  inline object_type& get_object() & { return this->template get<object_type>(); }

  inline object_type const& get_object() const & { return this->template get<object_type>(); }

  inline object_type get_object() && { return std::move(this->template get<object_type>()); }

  inline array_type& get_array() & { return this->template get<array_type>(); }

  inline array_type const& get_array() const & { return this->template get<array_type>(); }

  inline array_type get_array() && { return std::move(this->template get<array_type>()); }

  container& at(size_t value) & {
    if (!this->template is<array_type>())
      throw bad_container_array_access(
          "Bad container access in operator[](size_t), container is not an array.");
    return this->template get<array_type>()[value];
  }

  container const& at(size_t value) const & {
    if (!this->template is<array_type>())
      throw bad_container_array_access(
          "Bad container access in operator[](size_t), container is not an array.");
    return this->template get<array_type>()[value];
  }

  // Access without any check. Be careful with this one
  container& operator[](size_t value) & { return this->template raw<array_type>()[value]; }

  // Access without any check. Be careful with this one
  container const& operator[](size_t value) const & {
    return this->template raw<array_type>()[value];
  }

  container operator[](size_t value) && { return this->template get<array_type>()[value]; }

  template <class T, class Enabler = std::enable_if_t<!std::is_integral<std::decay_t<T>>(), void>>
  container& operator[](T&& value) & {
    if (!this->template is<object_type>())
      throw bad_container_object_access(
          "Bad container access in operator[](Key), container is not an object.");
    return this->template get<object_type>()[std::forward<T>(value)];
  }

  template <class T, class Enabler = std::enable_if_t<!std::is_integral<std::decay_t<T>>(), void>>
  container const& operator[](T&& value) const & {
    if (!this->template is<object_type>())
      throw bad_container_object_access(
          "Bad container access in operator[](Key), container is not an object.");
    auto it = this->template get<object_type>().find(std::forward<T>(value));
    if (it != this->template get<object_type>().end()) { return it->second; }
    throw bad_container_object_access(
        "Bad container access in operator[](Key), no element at this key.");
  }

  template <class T, class Enabler = std::enable_if_t<!std::is_integral<std::decay_t<T>>(), void>>
  container operator[](T&& value) && {
    if (!this->template is<object_type>())
      throw bad_container_object_access(
          "Bad container access in operator[](Key), container is not an object.");
    return std::move(this->template get<object_type>()[std::forward<T>(value)]);
  }
};

namespace helpers {
template <class Variant, class T>
bool equals(Variant const& lhs, Variant const& rhs) {
  return lhs.template raw<T>() == rhs.template raw<T>();
}

template <class Variant, class T>
bool less(Variant const& lhs, Variant const& rhs) {
  return lhs.template raw<T>() < rhs.template raw<T>();
}
};

template <class... Value>
bool operator==(variant<Value...> const& lhs, variant<Value...> const& rhs) {
  if (lhs.type_index() == rhs.type_index()) {
    static std::array<bool (*)(variant<Value...> const&, variant<Value...> const&),
                      sizeof...(Value)> appliers = {helpers::equals<variant<Value...>, Value>...};
    return appliers[lhs.type_index()](lhs, rhs);
  }
  return false;
}

template <class T, class... Value>
std::enable_if_t<!std::is_base_of<variant<Value...>, T>::value, bool> operator==(
    variant<Value...> const& lhs, T const& rhs) {
  if (lhs.template is<T>()) { return lhs.template raw<T>() == rhs; }
  return false;
}

template <class T, class... Value>
inline std::enable_if_t<!std::is_base_of<variant<Value...>, T>::value, bool> operator==(
    T const& lhs, variant<Value...> const& rhs) {
  return rhs == lhs;
}

template <class... Value>
inline bool operator!=(variant<Value...> const& lhs, variant<Value...> const& rhs) {
  return !(lhs == rhs);
}

template <class T, class... Value>
inline std::enable_if_t<!std::is_base_of<variant<Value...>, T>::value, bool> operator!=(
    variant<Value...> const& lhs, T const& rhs) {
  return !(lhs == rhs);
}

template <class T, class... Value>
inline std::enable_if_t<!std::is_base_of<variant<Value...>, T>::value, bool> operator!=(
    T const& lhs, variant<Value...> const& rhs) {
  return !(rhs == lhs);
}

template <class... Value>
bool operator<(variant<Value...> const& lhs, variant<Value...> const& rhs) {
  if (lhs.type_index() == rhs.type_index()) {
    static std::array<bool (*)(variant<Value...> const&, variant<Value...> const&),
                      sizeof...(Value)> appliers = {helpers::less<variant<Value...>, Value>...};
    return appliers[lhs.type_index()](lhs, rhs);
  }
  return lhs.type_index() < rhs.type_index();
}

// template <template <class> class Object, template <class> class Array, class Key, class... Value>
// inline bool operator==(container<Object, Array, Key, Value...> const& lhs,
// container<Object, Array, Key, Value...> const& rhs) {
// using variant_type = typename container<Object, Array, Key, Value...>::variant_type;
// return static_cast<variant_type const &>(lhs) == static_cast<variant_type const &>(rhs);
//}
//
// template <template <class> class Object, template <class> class Array, class Key, class... Value>
// inline bool operator!=(container<Object, Array, Key, Value...> const& lhs,
// container<Object, Array, Key, Value...> const& rhs) {
// return !(lhs == rhs);
//}
//
// template <template <class> class Object, template <class> class Array, class Key, class... Value>
// inline bool operator<(container<Object, Array, Key, Value...> const& lhs,
// container<Object, Array, Key, Value...> const& rhs) {
// using variant_type = typename container<Object, Array, Key, Value...>::variant_type;
// return static_cast<variant_type const &>(lhs) < static_cast<variant_type const &>(rhs);
//}

// element_init is used to write elegant jsonlike notations
template <class Container>
class element_init {
  typename Container::object_type::key_type key_;
  Container value_;

 public:
  using container_type = Container;
  element_init(typename Container::object_type::key_type const& key) : key_{key} {}
  element_init(typename Container::object_type::key_type&& key) : key_{std::move(key)} {}
  template <class T>
  typename Container::object_type::value_type operator=(T&& value) && {
    return {std::move(key_), std::forward<T>(value)};
  }
};

//
// Make an object out of an initializer list
//
// initialize_list is not default supported to avoid conflicts
// with bounded types that would support them.
//
template <class Container, class Key = typename Container::key_type>
Container make_object(std::initializer_list<std::pair<Key const, Container>> elements) {
  return elements;
}

//
// Make an array out of an initializer list
//
// initialize_list is not default supported to avoid conflicts
// with bounded types that would support them.
//
template <class Container>
Container make_array(std::initializer_list<Container> elements) {
  return elements;
}

namespace visiting_helpers {
// applier_maker generates function pointers
template <class Return, class... Value>
struct applier_maker;
template <class Return, class... Value>
struct applier_maker<Return, variant<Value...>> {
  template <class Visitor, class T, class... ExtraArguments>
  static Return applier_fp(variant<Value...>& values, Visitor visitor, ExtraArguments... extras) {
    return visitor(values.template raw<T>(), std::forward<ExtraArguments>(extras)...);
  }

  template <class Visitor, class T, class... ExtraArguments>
  static Return const_applier_fp(variant<Value...> const& values, Visitor visitor,
                                 ExtraArguments... extras) {
    return visitor(values.template raw<T>(), std::forward<ExtraArguments>(extras)...);
  }
};
// Extend to container
template <class Return, template <class...> class Object, template <class...> class Array,
          class Key, class... Value>
struct applier_maker<Return, container<Object, Array, Key, Value...>>
    : public applier_maker<Return, typename container<Object, Array, Key, Value...>::variant_type> {
  using container_type = container<Object, Array, Key, Value...>;
  using applier_maker<Return, typename container_type::variant_type>::applier_maker;
};
}  // namespace visiting_helpers

template <class Return, class Visitor, class... Value, class... ExtraArguments>
Return apply_visitor(variant<Value...>& values, Visitor&& visitor, ExtraArguments&&... extras) {
  static std::array<Return (*)(variant<Value...>&, std::add_lvalue_reference_t<Visitor>,
                               ExtraArguments...),
                    sizeof...(Value)> appliers = {
      visiting_helpers::applier_maker<Return, variant<Value...>>::template applier_fp<
          std::add_lvalue_reference_t<Visitor>, Value, ExtraArguments...>...};
  return appliers[values.type_index()](values, std::forward<Visitor>(visitor),
                                       std::forward<ExtraArguments>(extras)...);
};

template <class Return, class Visitor, class... Value, class... ExtraArguments>
Return apply_visitor(variant<Value...> const& values, Visitor&& visitor,
                     ExtraArguments&&... extras) {
  static std::array<Return (*)(variant<Value...> const&, std::add_lvalue_reference_t<Visitor>,
                               ExtraArguments...),
                    sizeof...(Value)> appliers = {
      visiting_helpers::applier_maker<Return, variant<Value...>>::template const_applier_fp<
          std::add_lvalue_reference_t<Visitor>, Value, ExtraArguments...>...};
  return appliers[values.type_index()](values, std::forward<Visitor>(visitor),
                                       std::forward<ExtraArguments>(extras)...);
};

//
// Visitor generated with functions
//
// This allows to write complex visitation without defining an object
//
template <class Return, class Variant, class FuncTuple, class... ExtraArguments>
struct func_aggregate_visitor_impl;
template <class Return, class Variant, class... FuncSignatures, class... ExtraArguments>
struct func_aggregate_visitor_impl<Return, Variant, std::tuple<FuncSignatures...>,
                                   ExtraArguments...> {
  using variant_type = Variant;
  std::tuple<FuncSignatures...> appliers;

  func_aggregate_visitor_impl(FuncSignatures... applier) : appliers{applier...} {}

  template <class T>
  Return operator()(T&& value, ExtraArguments... extras) const {
    return std::get<variant_type::target_type_list_t::template get_index<std::decay_t<T>>()>(
        appliers)(std::forward<T>(value), extras...);
  }
};

//
// Func aggregate visitor is an aggregate visitor implemented with std::function
//
// There is a performance penaly though because of std::function
//
template <class...>
struct func_aggregate_visitor;
template <class Return, class... Value, class... ExtraArguments>
struct func_aggregate_visitor<Return, variant<Value...>, ExtraArguments...>
    : func_aggregate_visitor_impl<Return, variant<Value...>,
                                  std::tuple<std::function<Return(Value&, ExtraArguments...)>...>,
                                  ExtraArguments...> {
  using func_aggregate_visitor_impl<Return, variant<Value...>,
                                    std::tuple<std::function<Return(Value&, ExtraArguments...)>...>,
                                    ExtraArguments...>::func_aggregate_visitor_impl;
};
// Specialization for containers
template <class Return, template <class...> class Object, template <class...> class Array,
          class Key, class... Value, class... ExtraArguments>
struct func_aggregate_visitor<Return, container<Object, Array, Key, Value...>, ExtraArguments...>
    : public func_aggregate_visitor<Return,
                                    typename container<Object, Array, Key, Value...>::variant_type,
                                    ExtraArguments...> {
  using func_aggregate_visitor<Return,
                               typename container<Object, Array, Key, Value...>::variant_type,
                               ExtraArguments...>::func_aggregate_visitor;
};

//
// Const func aggregate visitor is an aggregate visitor implemented with std::function
//
template <class...>
struct const_func_aggregate_visitor;
template <class Return, class... Value, class... ExtraArguments>
struct const_func_aggregate_visitor<Return, variant<Value...>, ExtraArguments...>
    : func_aggregate_visitor_impl<
          Return, variant<Value...>,
          std::tuple<std::function<Return(Value const&, ExtraArguments...)>...>,
          ExtraArguments...> {
  using func_aggregate_visitor_impl<
      Return, variant<Value...>,
      std::tuple<std::function<Return(Value const&, ExtraArguments...)>...>,
      ExtraArguments...>::func_aggregate_visitor_impl;
};
// Specialization for containers
template <class Return, template <class...> class Object, template <class...> class Array,
          class Key, class... Value, class... ExtraArguments>
struct const_func_aggregate_visitor<Return, container<Object, Array, Key, Value...>,
                                    ExtraArguments...>
    : public const_func_aggregate_visitor<
          Return, typename container<Object, Array, Key, Value...>::variant_type,
          ExtraArguments...> {
  using const_func_aggregate_visitor<Return,
                                     typename container<Object, Array, Key, Value...>::variant_type,
                                     ExtraArguments...>::const_func_aggregate_visitor;
};

//
// Func aggregate visitor is an aggregate visitor implemented with function pointers
//
// No penalty of std::function here, but casts from generic lambdas returning void
// wont work
//
template <class...>
struct funcptr_aggregate_visitor;
template <class Return, class... Value, class... ExtraArguments>
struct funcptr_aggregate_visitor<Return, variant<Value...>, ExtraArguments...>
    : func_aggregate_visitor_impl<Return, variant<Value...>,
                                  std::tuple<identity_t<Return (*)(Value&, ExtraArguments...)>...>,
                                  ExtraArguments...> {
  using func_aggregate_visitor_impl<
      Return, variant<Value...>, std::tuple<identity_t<Return (*)(Value&, ExtraArguments...)>...>,
      ExtraArguments...>::func_aggregate_visitor_impl;
};
// Specialization for containers
template <class Return, template <class...> class Object, template <class...> class Array,
          class Key, class... Value, class... ExtraArguments>
struct funcptr_aggregate_visitor<Return, container<Object, Array, Key, Value...>, ExtraArguments...>
    : public funcptr_aggregate_visitor<
          Return, typename container<Object, Array, Key, Value...>::variant_type,
          ExtraArguments...> {
  using funcptr_aggregate_visitor<Return,
                                  typename container<Object, Array, Key, Value...>::variant_type,
                                  ExtraArguments...>::funcptr_aggregate_visitor;
};

// Const func aggregate visitor is an aggregate visitor implemented with function pointers
template <class...>
struct const_funcptr_aggregate_visitor;
template <class Return, class... Value, class... ExtraArguments>
struct const_funcptr_aggregate_visitor<Return, variant<Value...>, ExtraArguments...>
    : func_aggregate_visitor_impl<
          Return, variant<Value...>,
          std::tuple<identity_t<Return (*)(Value const&, ExtraArguments...)>...>,
          ExtraArguments...> {
  using func_aggregate_visitor_impl<
      Return, variant<Value...>,
      std::tuple<identity_t<Return (*)(Value const&, ExtraArguments...)>...>,
      ExtraArguments...>::func_aggregate_visitor_impl;
};
// Specialization for containers
template <class Return, template <class...> class Object, template <class...> class Array,
          class Key, class... Value, class... ExtraArguments>
struct const_funcptr_aggregate_visitor<Return, container<Object, Array, Key, Value...>,
                                       ExtraArguments...>
    : public const_funcptr_aggregate_visitor<
          Return, typename container<Object, Array, Key, Value...>::variant_type,
          ExtraArguments...> {
  using const_funcptr_aggregate_visitor<
      Return, typename container<Object, Array, Key, Value...>::variant_type,
      ExtraArguments...>::const_funcptr_aggregate_visitor;
};

//
// base_converter show the API to be implemented to convert data in a view
//
//
struct base_converter {
  template <class Target, class Source,
            class Enable = std::enable_if_t<std::is_convertible<Source, Target>::value, void>>
  static Target convert(Source&& value) {
    return static_cast<Target>(std::forward<Source>(value));
  }

  template <class Target, class Source,
            class Enable = std::enable_if_t<!std::is_convertible<Source, Target>::value, void>,
            class Shim1 = void>
  static Target convert(Source&& value) {
    return {};
  }
};

template <class Return, class Converter, class Variant>
struct converter_maker;
template <class Return, class Converter, class... Value>
struct converter_maker<Return, Converter, variant<Value...>> {
  template <class Source>
  static Return converter_fp(Source const& value) {
    return Converter::template convert<Return>(value);
  }

  static const_funcptr_aggregate_visitor<Return, variant<Value...>> make_converter() {
    return {converter_fp<Value>...};
  };
};

struct bad_view_access : std::logic_error {
  using std::logic_error::logic_error;
};

template <class T>
class view_iterator;

//
// view is a contant non-owning view of a container
//
// The principle is similar to std::experimental::string_view
// The goal is to read through a container without generating exceptions if
// this is possible in case of missing keys or type mismatch
//
template <class Container, class Converter = base_converter>
class view {
  typename Container::object_type::key_type const* key_;
  Container const* container_;

  template <class T,
            class Enabler = std::enable_if_t<std::is_default_constructible<T>::value, void>>
  T const& inner_get(T const& default_value = {}) const & {
    if (container_ && container_->template is<T>()) return container_->template raw<T>();
    return default_value;
  }

  template <class T,
            class Enabler = std::enable_if_t<!std::is_default_constructible<T>::value, void>>
  T const& inner_get() const & {
    if (container_) return container_->template get<T>();
    throw bad_view_access("Access to en empty view in get().");
  }

  template <class T,
            class Enabler = std::enable_if_t<!std::is_default_constructible<T>::value, void>,
            class Shim1 = void>
  T const& inner_get(T const& default_value) const & {
    if (container_ && container_->template is<T>()) return container_->template raw<T>();
    return default_value;
  }

 public:
  using container_type = Container;
  using key_type = typename Container::object_type::key_type;
  using object_type = typename Container::object_type;
  using array_type = typename Container::array_type;
  using converter_type = Converter;

  view() noexcept : key_{nullptr}, container_{nullptr} {}
  view(Container const& container) noexcept : key_{nullptr}, container_{&container} {}
  view(typename Container::object_type::key_type const* key, Container const& container) noexcept
      : key_{key},
        container_{&container} {}
  view(view const&) noexcept = default;
  view(view&&) noexcept = default;
  view& operator=(view const&) noexcept = default;
  view& operator=(view&&) noexcept = default;

  // This is used to ease the use of range-base for loops
  key_type const& key() const & { return *key_; }

  bool empty() const& noexcept { return nullptr == container_; }

  view operator[](size_t value) const & {
    if (container_ && container_->template is<typename Container::array_type>()) {
      return view{container_->template get<typename Container::array_type>()[value]};
    }
    return {};
  }

  template <class T, class Enabler = std::enable_if_t<!std::is_integral<std::decay_t<T>>(), void>>
  view operator[](T&& value) const & {
    if (container_ && container_->template is<typename Container::object_type>()) {
      auto value_it =
          container_->template get<typename Container::object_type>().find(std::forward<T>(value));
      if (value_it != container_->template get<typename Container::object_type>().end()) {
        return view{value_it->second};
      }
    }
    return {};
  }

  template <class T>
  T const& get() const & {
    if (container_) return container_->template get<T>();
    throw bad_view_access("Access to en empty view in get().");
  }

  template <class T>
  T get(T&& default_value) const & {
    if (container_ && container_->template is<std::decay_t<T>>())
      return container_->template raw<std::decay_t<T>>();
    else
      return std::forward<T>(default_value);
  }

  template <class T>
  T as() const & {
    static_assert(std::is_default_constructible<T>::value,
                  "view::as shall be used with default constructible types only");
    static auto conversion_visitor =
        converter_maker<T, Converter, typename Container::variant_type>::make_converter();
    if (container_) return apply_visitor<T>(*container_, conversion_visitor);

    return {};
  }

  inline object_type const& get_object() const & { return this->template get<object_type>(); }
  inline array_type const& get_array() const & { return this->template get<array_type>(); }

  view_iterator<view> begin() const & {
    if (container_) {
      switch (container_->type_index()) {
        case (container_type::template type_index<object_type>()):
          return {*this, container_->template raw<object_type>().cbegin()};
        case (container_type::template type_index<array_type>()):
          return {*this, container_->template raw<array_type>().cbegin()};
        default:
          break;
      }
    }
    return {};
  }

  view_iterator<view> end() const & {
    if (container_) {
      switch (container_->type_index()) {
        case (container_type::template type_index<object_type>()):
          return {*this, container_->template raw<object_type>().cend()};
        case (container_type::template type_index<array_type>()):
          return {*this, container_->template raw<array_type>().cend()};
        default:
          break;
      }
    }
    return {};
  }

  inline view_iterator<view> cbegin() const & { return begin(); }

  inline view_iterator<view> cend() const & { return end(); }

  template <class T>
  inline bool is() const noexcept {
    return container_ ? container_->template is<T>() : false;
  }

  template <class T,
            class Enabler = std::enable_if_t<
                container_type::target_type_list_t::template has_type<std::decay_t<T>>(), void>>
  operator T() const & {
    return container_->template get<std::decay_t<T>>();
  }
};

template <class Container, class Converter = base_converter>
view<Container, Converter> make_view(Container const& container) {
  return view<Container, Converter>{container};
}

// Forward declaration of the iterator
template <class View>
class view_iterator;

template <class Container, class Converter>
class view_iterator<view<Container, Converter>> {
 public:
  using container_type = Container;
  using view_type = view<Container>;
  using object_iterator = typename container_type::object_type::const_iterator;
  using array_iterator = typename container_type::array_type::const_iterator;
  using value_type = variant<std::nullptr_t, array_iterator, object_iterator>;
  using converter_type = Converter;
  friend struct visiting_helpers::applier_maker<bool, value_type>;

 private:
  // This is the "parent" view holding the iterated collection
  view_type const* parent_view_;
  // This is the current iterator
  value_type value_;
  // This is a view built from each element at incrementation
  view_type current_view_;

 public:
  view_iterator() noexcept : parent_view_{nullptr}, value_{nullptr} {};

  view_iterator(view<container_type> const& parent, std::nullptr_t)
      : parent_view_{&parent}, value_{nullptr} {};

  view_iterator(view<container_type> const& parent, object_iterator&& value)
      : parent_view_{&parent}, value_{std::move(value)} {
    if (value != this->parent_view_->get_object().end())
      this->current_view_ = view_type(&value->first, value->second);
    else
      this->current_view_ = view_type();
  };

  view_iterator(view<container_type> const& parent, object_iterator const& value)
      : parent_view_{&parent}, value_{value} {
    if (value != this->parent_view_->get_object().end())
      this->current_view_ = view_type(&value->first, value->second);
    else
      this->current_view_ = view_type();
  };

  view_iterator(view<container_type> const& parent, array_iterator&& value)
      : parent_view_{&parent}, value_{std::move(value)} {
    if (value != this->parent_view_->get_array().end())
      this->current_view_ = view_type(*value);
    else
      this->current_view_ = view_type();
  };

  view_iterator(view<container_type> const& parent, array_iterator const& value)
      : parent_view_{&parent}, value_{value} {
    if (value != this->parent_view_->get_array().end())
      this->current_view_ = view_type(*value);
    else
      this->current_view_ = view_type();
  };

  // TODO: support no except here
  view_iterator(view_iterator const&) = default;
  view_iterator(view_iterator&&) = default;
  view_iterator& operator=(view_iterator const&) = default;
  view_iterator& operator=(view_iterator&&) = default;

  view_iterator& operator++() {
    static funcptr_aggregate_visitor<void, value_type, view_iterator&> const increment_visitor{
        [](std::nullptr_t&, view_iterator&) {},
        [](array_iterator& iterator, view_iterator& self) {
          using view_type = view<container_type, converter_type>;
          ++iterator;
          if (iterator != self.parent_view_->get_array().end())
            self.current_view_ = view_type(*iterator);
          else
            self.current_view_ = view_type();
        },
        [](object_iterator& iterator, view_iterator& self) {
          using view_type = view<container_type, converter_type>;
          ++iterator;
          if (iterator != self.parent_view_->get_object().end())
            self.current_view_ = view_type(&iterator->first, iterator->second);
          else
            self.current_view_ = view_type();
        }};
    apply_visitor<void>(this->value_, increment_visitor, *this);
    return *this;
  }

  view_iterator operator++(int) {
    view_iterator current{*this};
    ++(*this);
    return current;
  }

  view_type const& operator*() const { return this->current_view_; }
  view_type const* operator->() const { return &this->current_view_; }

  bool operator==(view_iterator const& compared) const & {
    static const_funcptr_aggregate_visitor<bool, value_type, view_iterator const&> const
        compare_visitor{[](std::nullptr_t const&, view_iterator const& other) {
                          return other.value_.template is<std::nullptr_t>();
                        },
                        [](array_iterator const& iterator, view_iterator const& other) {
                          // MSVC messes hard if the condition is not broken like this
                          bool a = (other.value_.template is<array_iterator>());
                          return (a && (iterator == other.value_.template raw<array_iterator>()));
                        },
                        [](object_iterator const& iterator, view_iterator const& other) {
                          // MSVC messes hard if the condition is not broken like this
                          bool a = (other.value_.template is<object_iterator>());
                          return (a && (iterator == other.value_.template raw<object_iterator>()));
                        }};

    return apply_visitor<bool, decltype(compare_visitor)&>(this->value_, compare_visitor, compared);
  }

  bool operator!=(view_iterator const& other) { return !(*this == other); }
};

}  // namespace json_backbone

#endif  // JSON_BACKBONE_BASE_HEADER
