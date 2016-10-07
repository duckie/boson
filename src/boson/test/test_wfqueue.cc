#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <cassert>
#include <vector>
#include "boson/queues/wfqueue.h"
#include "catch.hpp"
#include "wfqueue/wfqueue.h"
#include <pthread.h>

namespace {
  static constexpr size_t nb_iter = 1e4;
}

using namespace std;

TEST_CASE("Queues - WfQueue - simple case", "[queues][wfqueue]") {
  //boson::queues::wfqueue<int*> queue(1);
  //int i = 0;
  //void* none = queue.pop(0);
  //CHECK(none == nullptr);
  //queue.push(0,&i);
  //void* val = queue.pop(0);
  //CHECK(val == &i);
  //none = queue.pop(0);
  //CHECK(none == nullptr);
  //queue.push(0,nullptr);
  //none = queue.pop(0);
  //CHECK(none == nullptr);
  //queue_
}

TEST_CASE("Queues - WfQueue - sums", "[queues][wfqueue]") {
  constexpr size_t const nb_prod = 8;
  constexpr size_t const nb_cons = 8;
  constexpr size_t const nb_iter = 1e2;
  //pthread_barrier_t barrier;
  //pthread_barrier_init(&barrier, NULL, nb_prod+nb_cons);

  for(int j =0; j < 2; ++j) {
  size_t nnb_iter = nb_iter;
  //boson::queues::wfqueue<int*> queue(nb_prod + nb_cons);
  queue_t* q;
  handle_t** hds;
  q = static_cast<queue_t*>(align_malloc(PAGE_SIZE, sizeof(queue_t)));
  queue_init(q, nb_prod+nb_cons);
  hds = static_cast<handle_t**>(align_malloc(PAGE_SIZE, sizeof(handle_t * [nb_prod+nb_cons])));
  
  std::array<vector<size_t>, nb_prod> input {};
  std::array<size_t, nb_cons> output {};
  std::array<std::thread, nb_prod> input_th;
  std::array<std::thread, nb_cons> output_th;
  
  size_t expected = 0;
  for (size_t index = 0; index < nb_iter; ++index) {
    if (0 < index)
      input[index%nb_prod].push_back(index); 
    expected += index;
  }
//
  for (int index = 0; index < nb_prod; ++index) {
    input_th[index] = std::thread([&](int index){
      hds[index] = static_cast<handle_t*>(align_malloc(PAGE_SIZE, sizeof(handle_t)));
      queue_register(q, hds[index], index);
      //pthread_barrier_wait(&barrier);
        for(size_t i = 0; i < input[index].size(); ++i) {
          enqueue(q,hds[index],static_cast<void*>(&input[index][i]));
        }
        enqueue(q,hds[index],static_cast<void*>(&nnb_iter));
      //pthread_barrier_wait(&barrier);
      }, index);
  }
//
//
  for (int index = 0; index < nb_cons; ++index) {
    output_th[index] = std::thread([&](int index){
      hds[index+nb_prod] = static_cast<handle_t*>(align_malloc(PAGE_SIZE, sizeof(handle_t)));
      queue_register(q, hds[index+nb_prod], index);
      //pthread_barrier_wait(&barrier);
      size_t val = 0;
       do {
          val = 0;
          void* pval = dequeue(q,hds[index+nb_prod]);
          if (pval) {
            val = *static_cast<size_t*>(pval);
            if (val != nb_iter)
              output[index] += val;
            //delete static_cast<size_t*>(pval);
          }
        } while (val != nb_iter) ;
      //pthread_barrier_wait(&barrier);
    }, index);
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
  for(auto val : output)
    sum = sum + val;

  std::cout << sum << std::endl;

  CHECK(sum == expected);
  //pthread_barrier_destroy(&barrier);
  free(q);
  free(hds);
  }

}
