#ifndef BOSON_SELECT_H_
#define BOSON_SELECT_H_
#include "syscalls.h"
#include "channel.h"

namespace boson {

template <class Func>
class event_timer_storage {
  internal::routine_time_point target_; 
  Func func_;

 public:
  using func_type = Func;
  using return_type = decltype(std::declval<Func>()());

  static return_type execute(event_timer_storage * self) {
    return self->func_();
  }

  event_timer_storage(int timeout_ms, Func&& cb)
      : target_{std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(timeout_ms))},
        func_{std::move(cb)} {
  }

  event_timer_storage(int timeout_ms, Func const& cb)
      : target_{std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(timeout_ms))},
        func_{cb} {
  }

  bool subscribe(internal::routine* current) {
    current->add_timer(this->target_);
    return false;
  }
};

template <class Func> 
event_timer_storage<Func>
event_timer(int timeout_ms, Func&& cb) {
    return {timeout_ms,std::forward<Func>(cb)};
}

template <class Func>
class event_io_base_storage {
  protected:
    fd_t fd_;
    void* buf_;
    size_t count_;
    Func func_;

 public:
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()(std::declval<ssize_t>()));

    event_io_base_storage(fd_t fd, void* buf, size_t count, Func&& cb)
        : fd_{fd}, buf_{buf}, count_{count}, func_{std::move(cb)} {
    }

    event_io_base_storage(fd_t fd, void* buf, size_t count, Func const& cb)
        : fd_{fd}, buf_{buf}, count_{count}, func_{cb} {
    }
};

template <class Func>
class event_io_read_storage : public event_io_base_storage<Func> {
 public:
  using event_io_base_storage<Func>::event_io_base_storage;

  static typename event_io_base_storage<Func>::return_type execute(event_io_read_storage* self) {
    return self->func_(::read(self->fd_, self->buf_, self->count_));
  }

  bool subscribe(internal::routine* current) {
    current->add_read(this->fd_);
    return false;
  }
};

template <class Func> 
event_io_read_storage<Func>
event_read(fd_t fd, void* buf, size_t count, Func&& cb) {
    return {fd,buf,count,std::forward<Func>(cb)};
}

template <class Func>
class event_recv_storage : public event_io_base_storage<Func> {
  int flags_;

 public:
  static typename event_io_base_storage<Func>::return_type execute(event_recv_storage * self) {
    return self->func_(::recv(self->fd_, self->buf_, self->count_, self->flags_));
  }

  event_recv_storage(socket_t fd, void* buf, size_t count, int flags, Func&& cb)
      : event_io_base_storage<Func>{fd, buf, count, std::move(cb)}, flags_{flags} {
  }

  event_recv_storage(socket_t fd, void* buf, size_t count, int flags, Func const& cb)
      : event_io_base_storage<Func>{fd, buf, count, cb}, flags_{flags} {
  }

  bool subscribe(internal::routine* current) {
    current->add_read(this->fd_);
    return false;
  }
};

template <class Func> 
event_recv_storage<Func>
event_recv(fd_t fd, void* buf, size_t count, int flags, Func&& cb) {
    return {fd,buf,count,flags,std::forward<Func>(cb)};
}

template <class Func>
class event_accept_storage : public event_io_base_storage<Func> {
  socket_t socket_;
  sockaddr* address_;
  socklen_t* address_len_;
  Func func_;

 public:
  using func_type = Func;
  using return_type = decltype(std::declval<Func>()(std::declval<ssize_t>()));

  static typename event_io_base_storage<Func>::return_type execute(event_accept_storage * self) {
    return self->func_(::accept(self->socket_, self->address_, self->address_len_));
  }

  event_accept_storage(socket_t socket, sockaddr* address, socklen_t* address_len, Func&& cb)
      : socket_{socket}, address_{address}, address_len_{address_len}, func_{std::move(cb)} { 
  }

  event_accept_storage(socket_t socket, sockaddr* address, socklen_t* address_len, Func const& cb)
      : socket_{socket}, address_{address}, address_len_{address_len}, func_{cb} { 
  }

  bool subscribe(internal::routine* current) {
    current->add_read(this->socket_);
    return false;
  }
};

template <class Func> 
event_accept_storage<Func>
event_accept(socket_t socket, sockaddr* address, socklen_t* address_len, Func&& cb) {
    return {socket, address, address_len, std::forward<Func>(cb)};
}

template <class Func>
class event_io_write_storage : public event_io_base_storage<Func> {
 public:
  using event_io_base_storage<Func>::event_io_base_storage;

  static typename event_io_base_storage<Func>::return_type execute(event_io_write_storage* self) {
    return self->func_(::write(self->fd_, self->buf_, self->count_));
  }

  bool subscribe(internal::routine* current) {
    current->add_write(this->fd_);
    return false;
  }
};

template <class Func> 
event_io_write_storage<Func>
event_write(fd_t fd, void* buf, size_t count, Func&& cb) {
    return {fd,buf,count,std::forward<Func>(cb)};
}

template <class Func>
class event_send_storage : public event_io_base_storage<Func> {
  int flags_; 

 public:
  static typename event_io_base_storage<Func>::return_type execute(event_send_storage* self) {
    return self->func_(::send(self->socket_, self->buf_, self->count_, self->flags_));
  }

  event_send_storage(socket_t fd, void* buf, size_t count, int flags, Func&& cb)
      : event_io_base_storage<Func>{fd, buf, count, std::move(cb)}, flags_{flags} {
  }

  event_send_storage(socket_t fd, void* buf, size_t count, int flags, Func const& cb)
      : event_io_base_storage<Func>{fd, buf, count, cb}, flags_{flags} {
  }

  bool subscribe(internal::routine* current) {
    current->add_write(this->socket_);
    return false;
  }
};

template <class Func> 
event_send_storage<Func>
event_send(fd_t fd, void* buf, size_t count, int flags, Func&& cb) {
    return {fd,buf,count, flags, std::forward<Func>(cb)};
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
      int result = semaphore.impl_->counter_.fetch_sub(1, std::memory_order_acquire);
      if (result <= 0) {
        current->add_semaphore_wait(semaphore.impl_.get());
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

template <class ContentType, std::size_t Size, class Func>
class event_channel_write_storage {
    channel<ContentType,Size>& channel_;
    ContentType value_;
    Func func_;

 public:
    using channel_type = channel<ContentType,Size>;
    using func_type = Func;
    using return_type = decltype(std::declval<Func>()());

    static return_type execute(event_channel_write_storage* self) {
        self->channel_.consume_write(std::move(self->value_));
        return self->func_();
    }

    event_channel_write_storage(channel_type& channel, ContentType value, Func&& cb)
        : channel_{channel}, value_{value}, func_{std::move(cb)} {
    }

    event_channel_write_storage(channel_type& channel, ContentType value, Func const& cb)
        : channel_{channel}, value_{value}, func_{cb} {
    }

    bool subscribe(internal::routine* current) {
      auto& semaphore = channel_.channel_->writer_slots_;
      int result = semaphore.impl_->counter_.fetch_sub(1, std::memory_order_acquire);
      if (result <= 0) {
        current->add_semaphore_wait(semaphore.impl_.get());
        return false;
      }
      return true;
    }
};

template <class ContentType, std::size_t Size, class Func>
event_channel_write_storage<ContentType, Size, Func>
event_write(channel<ContentType,Size>& chan, ContentType value, Func&& cb) {
    return {chan, std::move(value), std::forward<Func>(cb)};
}

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
