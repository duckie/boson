#include "engine.h"
#include <iostream>

namespace boson {

void engine::push_command(thread_id from, std::unique_ptr<command> new_command) {
  command_queue_.push(from, new_command.release());
  command_waiter_.notify_one();
}

void engine::execute_commands() {
  std::mutex mut;
  std::unique_lock<std::mutex> lock(mut);
  std::lock_guard<decltype(lock)> guard(lock);
      command_waiter_.wait(lock);

  while (0 < nb_active_threads_) {
    std::unique_ptr<command> new_command;
    do {
      new_command.reset(command_queue_.pop(0));
      if (new_command) {
        switch (new_command->type) {
          case command_type::add_routine: {
            thread_id target_thread;
            std::unique_ptr<internal::routine> new_routine;
            tie(target_thread, new_routine) = move(new_command->data.raw<command_new_routine_data>());
            if (target_thread == max_nb_cores_) {
              target_thread = next_scheduled_thread_++;
              next_scheduled_thread_ %= max_nb_cores_;
            }
            threads_.at(target_thread)->thread.push_command(max_nb_cores_,
                command_t{internal::thread_command_type::add_routine, move(new_routine)});
          } break;
          case command_type::notify_end_of_thread: {
            --nb_active_threads_;
          } break;
        }
      }

    } while(new_command);
    if (0 < nb_active_threads_) {
      command_waiter_.wait(lock);
    }
  }
}

thread_id engine::register_thread_id() {
  auto new_id = current_thread_id_++;
  return new_id;
}

engine::engine(size_t max_nb_cores) : max_nb_cores_{max_nb_cores}, nb_active_threads_{max_nb_cores}, command_queue_{static_cast<int>(max_nb_cores+1)} {
  // Start all threads directly
  threads_.reserve(max_nb_cores);
  for (size_t index = 0; index < max_nb_cores_; ++index) {
    threads_.emplace_back(new thread_view_t(*this));
    auto& created_thread = threads_.back();
    threads_.back()->std_thread =
        std::thread([&created_thread]() { created_thread->thread.loop(); });
  }
}

engine::~engine() {
  // Send a request to thread to finish when they can
  for (auto& thread : threads_) {
    thread->thread.push_command(max_nb_cores_, command_t{internal::thread_command_type::finish, nullptr});
    // thread->thread.execute_commands();
  }

  // Loop
  execute_commands();
        std::cout << "wtf" << std::endl;

  // Join everyone
  for (auto& thread : threads_) {
    thread->std_thread.join();
  }
};
}  // namespace boson
