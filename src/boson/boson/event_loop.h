#ifndef BOSON_EVENTLOOP_H_
#define BOSON_EVENTLOOP_H_
#pragma once

#include <cstdint>
#include <memory>

namespace boson {

struct event_handler {
  virtual void event(int event_id, void* data) = 0;
  virtual void read(int fd, void* data) = 0;
  virtual void write(int fd, void* data) = 0;
};

enum class loop_end_reason {
  max_iter_reached,
  timed_out,
  error_occured
};

/**
 * Platform specific loop implementation
 *
 * We are using a envelop/letter pattern to hide
 * implementation into platform specific code.
 */
class event_loop_impl;

/**
 * event_loop is the class encapsulating the asynchronous I/O loop
 *
 * Since implementation of this loop is platform dependent
 * it must be outside other classes to ease platform specific
 * code integration.
 *
 */
class event_loop {
  std::unique_ptr<event_loop_impl> loop_impl_;

 public:
  ~event_loop();

  /**
   * Creates an event loop
   *
   * An event loop is supposed to be alone to loop
   * into the current thread. Therefore we can store
   * a handler which will stay the same over the loop
   * lifespan
   *
   */
  event_loop(event_handler& handler);

  /**
   * Registers for a long running event listening
   *
   * Unlike read and writes, resgister_event does not work like
   * a one shot request but will live until unregistered
   */
  int register_event(void* data);

  /**
   * Triggers a registered event
   *
   * This can be called from another thread than the main
   * thread of the loop
   */
  void send_event(int event);

  /**
   * Listen a fd for read
   *
   * Those events are one time_events, they must be rearmed each time
   * It comes from the fact that the routines decide if an FD must
   * be listened to or not, and there is no way we ask the user to decide
   * in advance. so every "blocking" sys call in a routine rearms the event.
   */
  int register_read(int fd, void* data);

  /**
   * Listen a fd for write
   *
   * Those events are one time_events, they must be rearmed each time
   * It comes from the fact that the routines decide if an FD must
   * be listened to or not, and there is no way we ask the user to decide
   * in advance. so every "blocking" sys call in a routine rearms the event.
   */
  int register_write(int fd, void* data);

  /**
   * disables the event without deleting it
   */
  void disable(int event_id);

  /**
   * re-enables a previosuly disabled event
   */
  void enable(int event_it);

  /**
   * Unregister the event and give back its data
   */
  void* unregister(int event_id);

  /**
   * Executes the event loop
   *
   * In fact, we only execute one iteration because the user of this 
   * class (boson::thread) executes private things between each iteration
   */
  loop_end_reason loop(int max_iter = -1, int timeout_ms = -1);
};
};

#endif  // BOSON_EVENTLOOP_H_
