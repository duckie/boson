#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include "boson/boson.h"
#include "boson/logger.h"
#include "boson/mutex.h"

using namespace std::literals;
using namespace std::chrono;
using namespace std;

static constexpr int nb_iter = 1e6;
static constexpr int nb_threads = 4;
constexpr size_t const nb_prod = 16;
constexpr size_t const nb_cons = 16;

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);


  size_t nnb_iter = nb_iter;
  boson::queues::wfqueue<int*> queue(nb_threads+1);

  std::array<vector<size_t>, nb_prod> input{};
  std::array<size_t, nb_cons> output{};
  std::array<std::thread, nb_prod> input_th;
  std::array<std::thread, nb_cons> output_th;

  size_t expected = 0;
  for (size_t index = 0; index < nb_iter; ++index) {
    if (0 < index) input[index % nb_prod].push_back(index);
    expected += index;
  }
  // Execute a routine communication through pipes
  {
    boson::engine instance(nb_threads);
    for (int index = 0; index < nb_prod; ++index) {
      instance.start([&, index]() {
        for (size_t i = 0; i < input[index].size(); ++i) {
          queue.push(boson::internal::current_thread()->id(), static_cast<void*>(&input[index][i]));
        }
        queue.push(boson::internal::current_thread()->id(), static_cast<void*>(&nnb_iter));
      });
    }
    for (int index = 0; index < nb_cons; ++index) {
      instance.start([&, index]() {
        size_t val = 0;
        do {
          val = 0;
          void* pval = queue.pop(boson::internal::current_thread()->id());
          if (pval) {
            val = *static_cast<size_t*>(pval);
            if (val != nb_iter) output[index] += val;
          }
        } while (val != nb_iter);
      });
    }
  }
  size_t sum = 0;
  for (auto val : output) sum = sum + val;

  assert(sum == expected);
}
