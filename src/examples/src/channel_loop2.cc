#include "boson/channel.h"
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
static constexpr int nb_threads = 16;
constexpr size_t const nb_prod = 112;
constexpr size_t const nb_cons = 112;

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);
  std::array<vector<size_t>, nb_prod> input{};
  std::array<size_t, nb_cons> output{};
  std::array<std::thread, nb_prod> input_th;
  std::array<std::thread, nb_cons> output_th;

  size_t expected = 0;
  for (size_t index = 0; index < nb_iter; ++index) {
    if (0 < index) input[index % nb_prod].push_back(index);
    expected += index;
  }
  {
    boson::run(nb_threads, [&]() {
      using namespace boson;
      channel<int, nb_iter> chan;
      for (int index = 0; index < nb_prod; ++index) {
        start(
            [&, index](auto chan) {
              for (size_t i = 0; i < input[index].size(); ++i) {
                chan << input[index][i];
              }
              chan << nb_iter;
              // queue.write(boson::internal::current_thread()->id(), static_cast<void*>(&nnb_iter));
            },
            dup(chan));
      }
      for (int index = 0; index < nb_cons; ++index) {
        start(
            [&, index](auto chan) {
              int val = 0;
              do {
                val = 0;
                if (chan >> val) {
                  if (val != nb_iter) output[index] += val;
                }
              } while (val != nb_iter);
            },
            dup(chan));
      }
    });
  }
  size_t sum = 0;
  for (auto val : output) sum = sum + val;

  assert(sum == expected);
}
