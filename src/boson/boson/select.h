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

    bool subscribe(internal::routine* current) {
        current->add_read(fd_);
        return false;
    }
};

template <class Func> 
event_read_storage<Func>
event_read(fd_t fd, void* buf, size_t count, Func&& cb) {
    return {fd,buf,count,std::forward<Func>(cb)};
}

template <class ContentType, std::size_t Size, class Func>
class event_channel_read_storage {
    channel<ContentType,Size>& channel_;
    ContentType& value_;
    Func func_;

 public:
    using channel_type = channel<ContentType,Size>;
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()());

    static return_type execute(event_channel_read_storage* self) {
        self->channel_.consume_read(self->value_);
        return self->func_();
    }

    event_channel_read_storage(channel_type& channel, ContentType& value, Func&& cb)
        : channel_{channel}, value_{value}, func_{std::move(cb)} {
    }

    event_channel_read_storage(channel_type& channel, ContentType& value, Func const& cb)
        : channel_{channel}, value_{value}, func_{cb} {
    }

    bool subscribe(internal::routine* current) {
      auto& semaphore = channel_.channel_->readers_slots_;
      int result = semaphore.counter_.fetch_sub(1, std::memory_order_acquire);
      if (result <= 0) {
        current->add_semaphore_wait(&semaphore);
        return false;
      }
      return true;
    }
};

template <class ContentType, std::size_t Size, class Func>
event_channel_read_storage<ContentType, Size, Func>
event_read(channel<ContentType,Size>& chan, ContentType& value, Func&& cb) {
    return {chan, value, std::forward<Func>(cb)};
}


//template <class ... T>
//inline void sink(T ...) {}

template <class Selector, class ReturnType> 
auto make_selector_execute() -> decltype(auto) {
  return [](void* data) -> ReturnType {
    return Selector::execute(static_cast<Selector*>(data));
  };
}

template <class Selector> 
auto make_selector_subscribe() -> decltype(auto) {
  return [](void* data, internal::routine* current) -> bool {
    return static_cast<Selector*>(data)->subscribe(current);
  };
}

template <class ... Selectors> 
auto select_any(Selectors&& ... selectors) 
    -> std::common_type_t<typename Selectors::return_type ...>
{
  using return_type = std::common_type_t<typename Selectors::return_type...>;
  static std::array<bool(*)(void*, internal::routine*), sizeof...(Selectors)> subscribers{
      make_selector_subscribe<Selectors>()...};
  static std::array<return_type (*)(void*), sizeof...(Selectors)> callers{
      make_selector_execute<Selectors, return_type>()...};
  std::array<void*, sizeof...(Selectors)> selector_ptrs{&selectors...};
  //std::array<void*, sizeof ... (Selectors)> selector_ptrs {&selectors...};
    

  internal::thread* this_thread = internal::current_thread();
  internal::routine* current_routine = this_thread->running_routine();
  current_routine->start_event_round();

  bool cancel = false;
  size_t index = 0;
  for (; index < sizeof ... (Selectors); ++index) {
    cancel = (*subscribers[index])(selector_ptrs[index],current_routine);
    if (cancel)
        break;
  }
  if (cancel) {
    current_routine->cancel_event_round();
  }
  else {
    current_routine->commit_event_round();
    index = current_routine->happened_index();
  }
  return (*callers[index])(selector_ptrs[index]);
}



};  // namespace boson

#endif  // BOSON_SELECT_H_
