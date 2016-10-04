#include "engine.h"

namespace boson {

void engine::push_command(thread_id from, std::unique_ptr<command> new_command) {
  command_pushers_.fetch_add(std::memory_order_release);
  command_waiter_.notify_one();
  command_queue_.push(from, new_command.release());
  command_waiter_.notify_one();
}

void engine::execute_commands() {
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
          auto& view = *threads_.at(target_thread);
          ++view.nb_routines;
          view.thread.push_command(
              max_nb_cores_,
              command_t{internal::thread_command_type::add_routine, move(new_routine)});
        } break;
        case command_type::notify_idle: {
          auto& view = *threads_.at(new_command->from);
          view.nb_routines = new_command->data.get<size_t>();
        } break;
        case command_type::notify_end_of_thread: {
          --nb_active_threads_;
        } break;
      }
      command_pushers_.fetch_sub(std::memory_order_release);
    }

  } while (new_command || 0 < this->command_pushers_.load(std::memory_order_acquire));
}
void engine::wait_all_routines() {
  std::mutex mut;
  std::unique_lock<std::mutex> lock(mut);

  while (0 < nb_active_threads_) {
    execute_commands();
    size_t nb_remaining_routines = 0;
    for(auto& view_ptr : threads_){
      nb_remaining_routines += view_ptr->nb_routines;
    }
    if (0 == nb_remaining_routines) {
      for (auto& thread : threads_) {
        thread->thread.push_command(max_nb_cores_,
                                    command_t{internal::thread_command_type::finish, nullptr});
      }
    }
    command_waiter_.wait(lock, [this] {
        return 0 == this->nb_active_threads_ || 0 < this->command_pushers_.load(std::memory_order_acquire);
    });
  }
}

thread_id engine::register_thread_id() {
  auto new_id = current_thread_id_++;
  return new_id;
}

engine::engine(size_t max_nb_cores) : max_nb_cores_{max_nb_cores}, nb_active_threads_{max_nb_cores}, command_queue_{static_cast<int>(max_nb_cores+1)}, command_pushers_{0} {
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
  wait_all_routines();

  // Join everyone
  for (auto& thread : threads_) {
    thread->std_thread.join();
  }
};
}  // namespace boson
