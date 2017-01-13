#ifndef BOSON_EVENTLOOP_H_
#define BOSON_EVENTLOOP_H_
#pragma once

#include <cstdint>
#include <memory>
#include <tuple>

namespace boson {

enum class event_status { ok, panic };

struct event_handler {
  virtual void event(int event_id, void* data, event_status status) = 0;
  virtual void read(int fd, void* data, event_status status) = 0;
  virtual void write(int fd, void* data, event_status status) = 0;
};

enum class loop_end_reason { max_iter_reached, timed_out, error_occured };

/**
 * Platform specific loop implementation
 *
 * We are using a envelop/letter pattern to hide
 * implementation into platform specific code.
 */
class event_loop;

///**
 //* event_loop is the class encapsulating the asynchronous I/O loop
 //*
 //* Since implementation of this loop is platform dependent
 //* it must be outside other classes to ease platform specific
 //* code integration.
 //*
 //*/
//class event_loop {
  //std::unique_ptr<LoopImpl> loop_impl_;
//
 //public:
  //~event_loop();
//
  ///**
   //* Creates an event loop
   //*
   //* An event loop is supposed to be alone to loop
   //* into the current thread. Therefore we can store
   //* a handler which will stay the same over the loop
   //* lifespan
   //*
   //*/
  //event_loop(event_handler& handler, int nprocs);
//
  ///**
   //* Registers for a long running event listening
   //*
   //* Unlike read and writes, resgister_event does not work like
   //* a one shot request but will live until unregistered
   //*/
  //int register_event(void* data);
//
  ///**
   //* Read data currectly registered for this event
   //*
   //* Event must exist or be the null event (-1), otherwise behavior is undefined
   //*/
  //void* get_data(int event_id);
//
  ///**
   //* Returns events attached to a given fd
   //*/
  //std::tuple<int,int> get_events(int fd);
//
  ///**
   //* Triggers a registered event
   //*
   //* This can be called from another thread than the main
   //* thread of the loop
   //*/
  //void send_event(int event);
//
  ///**
   //* Listen a fd for read
   //*/
  //int register_read(int fd, void* data);
//
  ///**
   //* Listen a fd for write
   //*/
  //int register_write(int fd, void* data);
//
  ///**
   //* disables the event without deleting it
   //*/
  //void disable(int event_id);
//
  ///**
   //* re-enables a previosuly disabled event
   //*/
  //void enable(int event_id);
//
  ///**
   //* Unregister the event and give back its data
   //*/
  //void* unregister(int event_id);
//
  ///**
   //* Force loop unlock for a given fd and a given event
   //*
   //* This can be useful to interrupt listening servers
   //*/
  //void send_fd_panic(int proc_from, int fd);
//
  ///**
   //* Executes the event loop
   //*
   //* In fact, we only execute one iteration because the user of this
   //* class (boson::thread) executes private things between each iteration
   //*/
  //loop_end_reason loop(int max_iter = -1, int timeout_ms = -1);
//};
};

extern template class std::unique_ptr<boson::event_loop>;

#endif  // BOSON_EVENTLOOP_H_
