#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include "boson/boson.h"
#include "boson/logger.h"
#include "boson/mutex.h"
#include "boson/queues/wfqueue.h"
#include "boson/queues/simple.h"
#include "boson/queues/lcrq.h"

using namespace std::literals;
using namespace std::chrono;
using namespace std;

static constexpr int nb_iter = 1e5;
static constexpr int nb_threads = 7;
constexpr size_t const nb_prod = 12;
constexpr size_t const nb_cons = 12;

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);

  size_t nnb_iter = nb_iter;
  // boson::queues::base_wfqueue queue(nb_threads);
  //boson::queues::base_wfqueue queue(nb_prod+nb_cons);
  boson::queues::lcrq queue(nb_prod+nb_cons);
  //boson::queues::simple_queue queue(
      //0);  // Just to bugfix, this example does not work and exposes an unsolved bug

  std::array<vector<size_t>, nb_prod> input{};
  std::array<size_t, nb_cons> output{};
  std::array<std::thread, nb_prod> input_th;
  std::array<std::thread, nb_cons> output_th;

  size_t expected = 0;
  size_t thread_index = 0;
  for (size_t index = 0; index < nb_iter; ++index) {
    if (0 < index) input[index % nb_prod].push_back(index);
    expected += index;
  }
  {
    boson::engine instance(nb_threads);
    for (int index = 0; index < nb_prod; ++index) {
      instance.start(
          [&, index](int thread) {
            for (size_t i = 0; i < input[index].size(); ++i) {
              // queue.write(boson::internal::current_thread()->id(),
              // static_cast<void*>(&input[index][i]));
              queue.write(index, static_cast<void*>(&input[index][i]));
              // queue.write(thread, static_cast<void*>(&input[index][i]));
            }
            // queue.write(boson::internal::current_thread()->id(), static_cast<void*>(&nnb_iter));
            queue.write(index, static_cast<void*>(&nnb_iter));
            // queue.write(thread, static_cast<void*>(&nnb_iter));
          },
          thread_index);
      thread_index = (thread_index + 1) % nb_threads;
    }
    for (int index = 0; index < nb_cons; ++index) {
      instance.start(
          [&, index](int thread) {
            size_t val = 0;
            do {
              val = 0;
              // void* pval = queue.read(boson::internal::current_thread()->id());
              void* pval = queue.read(index);
              // void* pval = queue.read(thread);
              if (pval) {
                val = *static_cast<size_t*>(pval);
                if (val != nb_iter) output[index] += val;
              }
            } while (val != nb_iter);
          },
          thread_index);
      thread_index = (thread_index + 1) % nb_threads;
    }
  }
  size_t sum = 0;
  for (auto val : output) sum = sum + val;

  std::cout << sum << "\n" << expected << std::endl;
  assert(sum == expected);
}
