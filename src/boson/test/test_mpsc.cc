#include <pthread.h>
#include <cassert>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <vector>
#include <mutex>
#include "boson/queues/mpsc.h"
#include "catch.hpp"

namespace {
static constexpr size_t nb_iter = 1e5 + 153;
}

using namespace std;

TEST_CASE("Queues - MPSC - simple case", "[queues][mpsc]") {
   boson::queues::mpsc<int> queue;
   int i = 0;
   bool result = queue.read(i);
   CHECK(!result);
   queue.write(1);
   result = queue.read(i);
   CHECK(result);
   CHECK(i == 1);
   result = queue.read(i);
   CHECK(!result);
   CHECK(i == 1);
}
/*
TEST_CASE("Queues - WfQueue - sums", "[queues][lcrq]") {
  constexpr size_t const nb_prod = 16;
  constexpr size_t const nb_cons = 16;
  constexpr size_t const nb_iter = 1e3;
  // pthread_barrier_t barrier;
  // pthread_barrier_init(&barrier, NULL, nb_prod+nb_cons);
  constexpr size_t const nb_main_threads = 4;

  std::array<std::thread, nb_main_threads> main_threads;
  std::mutex test_protection;
  for (size_t j = 0; j < nb_main_threads; ++j) {
    main_threads[j] = std::thread([&]() {
      size_t nnb_iter = nb_iter;
      // boson::queues::base_wfqueue queue(nb_prod + nb_cons);
      boson::queues::lcrq queue(nb_prod + nb_cons);

      std::array<vector<size_t>, nb_prod> input{};
      std::array<size_t, nb_cons> output{};
      std::array<std::thread, nb_prod> input_th;
      std::array<std::thread, nb_cons> output_th;

      size_t expected = 0;
      for (size_t index = 0; index < nb_iter; ++index) {
        if (0 < index) input[index % nb_prod].push_back(index);
        expected += index;
      }
      //
      for (size_t index = 0; index < nb_prod; ++index) {
        input_th[index] = std::thread(
            [&](int index) {
              for (size_t i = 0; i < input[index].size(); ++i) {
                queue.write(index, static_cast<void*>(&input[index][i]));
              }
              queue.write(index, static_cast<void*>(&nnb_iter));
            },
            index);
      }
      //
      //
      for (size_t index = 0; index < nb_cons; ++index) {
        output_th[index] = std::thread(
            [&](int index) {
              size_t val = 0;
              do {
                val = 0;
                void* pval = queue.read(index + nb_prod);
                if (pval) {
                  val = *static_cast<size_t*>(pval);
                  if (val != nb_iter) output[index] += val;
                }
              } while (val != nb_iter);
            },
            index);
      }
      //
      for (size_t index = 0; index < nb_prod; ++index) {
        input_th[index].join();
      }
      //
      for (size_t index = 0; index < nb_cons; ++index) {
        output_th[index].join();
      }
      //
      size_t sum = 0;
      for (auto val : output) sum = sum + val;

      std::lock_guard<std::mutex> test_guard{test_protection};
      CHECK(sum == expected);
    });
  }
  for (size_t j = 0; j < nb_main_threads; ++j) {
    main_threads[j].join();
  }
}
*/
