#include "catch.hpp"
#include "boson/event_loop.h"
#include <thread>


struct handler01 : public boson::event_handler {
  int last_id{0};
  void* last_data{nullptr};

  void event(int event_id, void* data) override {
    last_id = event_id;
    last_data = data;
  }
  void read(int fd, void* data) override {
  }
  void write(int fd, void* data) override {
  }
};

TEST_CASE("Event Loop - Event notification", "[eventloop][notif]") {
  handler01 handler_instance;

  boson::event_loop loop(handler_instance);
  int event_id = loop.register_event(nullptr);
  
  std::thread t1{[&loop]() {
    loop.loop(1);
  }};

  loop.send_event(event_id);
  t1.join();

  CHECK(handler_instance.last_id == event_id);
  CHECK(handler_instance.last_data == nullptr);
}
