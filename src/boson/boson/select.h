#ifndef BOSON_SELECT_H_
#define BOSON_SELECT_H_

#include "syscalls.h"
#include "channel.h"

namespace boson {

template <class Func>
class event_read_storage {
    fd_t fd_;
    void* buf_;
    size_t count_;
    Func func_;

 public:
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()(std::declval<ssize_t>()));

    static return_type execute(event_read_storage* self) {
        return self->func_(::read(self->fd_, self->buf_, self->count_));
    }

    event_read_storage(fd_t fd, void* buf, size_t count, Func&& cb)
        : fd_{fd}, buf_{buf}, count_{count}, func_{std::move(cb)} {
    }

    event_read_storage(fd_t fd, void* buf, size_t count, Func const& cb)
        : fd_{fd}, buf_{buf}, count_{count}, func_{cb} {
    }

    void subscribe(internal::routine* current) {
        current->add_read(fd_);
    }
};

template <class Func> 
event_read_storage<Func>
event_read(fd_t fd, void* buf, size_t count, Func&& cb) {
    return {fd,buf,count,std::forward<Func>(cb)};
}

template <class ... T>
inline void sink(T ...) {}

template <class Selector, class ReturnType> 
auto make_selector_execute() -> decltype(auto) {
  return [](void* data) -> ReturnType {
    return Selector::execute(static_cast<Selector*>(data));
  };
}

template <class ... Selectors> 
auto select_any(Selectors&& ... selectors) 
    -> std::common_type_t<typename Selectors::return_type ...>
{
  using return_type = std::common_type_t<typename Selectors::return_type...>;
  static std::array<return_type (*)(void*), sizeof...(Selectors)> callers{
      make_selector_execute<Selectors, return_type>()...};
  std::array<void*, sizeof...(Selectors)> selector_ptrs{&selectors...};
  //std::array<void*, sizeof ... (Selectors)> selector_ptrs {&selectors...};
    

  internal::thread* this_thread = internal::current_thread();
  internal::routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();
  sink((selectors.subscribe(current_routine),0)...);
  current_routine->commit_event_round();

  // TODO: make it work with any calling convention
  // Here we suppose cdecl is used
  size_t index = sizeof ... (Selectors) - current_routine->happened_index() - 1;
  return (*callers[index])(selector_ptrs[index]);
}



};  // namespace boson

#endif  // BOSON_SELECT_H_
