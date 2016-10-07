#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <cassert>
#include <vector>
#include "boson/queues/wfqueue.h"
#include "catch.hpp"

namespace {
  static constexpr size_t nb_iter = 1e4;
}

using namespace std;

TEST_CASE("Queues - WfQueue - simple case", "[queues][wfqueue]") {
  boson::queues::wfqueue<int*> queue(1);
  int i = 0;
  void* none = queue.pop(0);
  CHECK(none == nullptr);
  queue.push(0,&i);
  void* val = queue.pop(0);
  CHECK(val == &i);
  none = queue.pop(0);
  CHECK(none == nullptr);
  queue.push(0,nullptr);
  none = queue.pop(0);
  CHECK(none == nullptr);
  //queue_
}

TEST_CASE("Queues - WfQueue - sums", "[queues][wfqueue]") {
  constexpr size_t const nb_threads = 8;
  constexpr size_t const nb_iter = 1e2;
  size_t nnb_iter = nb_iter;
  boson::queues::wfqueue<int*> queue(nb_threads);
  std::array<vector<size_t>, nb_threads> input;
  std::array<size_t, nb_threads> output;
  std::array<std::thread, nb_threads> input_th;
  std::array<std::thread, nb_threads> output_th;
  
  size_t expected = 0;
  for (size_t index = 0; index < nb_iter; ++index) {
    if (0 < index)
      input[index%nb_threads].push_back(index); 
    expected += index;
  }

  for (size_t index = 0; index < nb_threads; ++index) {
    input_th[index] = std::thread([&,index](){
        for(size_t i = 0; i < input[index].size(); ++i) {
          queue.push(index,static_cast<void*>(&input[index][i]));
        }
        queue.push(index, static_cast<void*>(&nnb_iter));
      });
  }

  for (size_t index = 0; index < nb_threads; ++index) {
    input_th[index].join();
  }

  for (size_t index = 0; index < nb_threads; ++index) {
    output_th[index] = std::thread([&,index](){
      size_t val = 0;
       do {
          void* pval = queue.pop(index);
          if (pval) {
            val = *static_cast<size_t*>(pval);
            if (val != nb_iter)
              output[index] += val;
            //delete static_cast<size_t*>(pval);
          }
        } while (val != nb_iter) ;
    });
  }


  for (size_t index = 0; index < nb_threads; ++index) {
    output_th[index].join();
  }

  size_t sum = 0;
  for(auto val : output)
    sum += val;

  assert(sum == expected);

}
