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

static constexpr int nb_iter = 1e4;
static constexpr int nb_threads = 24;

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);

   auto t1 = high_resolution_clock::now();
  {
   std::vector<int> data;
   std::vector<int> data2;
   //Execute a routine communication through pipes
   std::mutex mut;
   std::mutex mut2;
   boson::engine instance(8);
   for (int i = 0; i < nb_threads / 2; ++i) {
     instance.start([&mut, &data]() mutable {
       for (int j = 0; j < nb_iter; ++j) {
         mut.lock();
         data.push_back(1);
         //// std::this_thread::sleep_for(2ms);
         mut.unlock();
       }
     });
     instance.start([&mut2, &data2]() mutable {
       for (int j = 0; j < nb_iter; ++j) {
         mut2.lock();
         data2.push_back(1);
         // boson::sleep(2ms);
         mut2.unlock();
       }
     });
   }
   t1 = high_resolution_clock::now();
  }
  auto t2 = high_resolution_clock::now();
  {
    std::vector<int> data;
    std::vector<int> data2;
    // Execute a routine communication through pipes
    boson::mutex mut;   // if not shared, it must outlive the engine instance
    boson::mutex mut2;  // if not shared, it must outlive the engine instance
    boson::engine instance(8);
    // boson::shared_mutex mut;  // if not shared, it must outlive the engine instance
    // boson::shared_mutex mut2;  // if not shared, it must outlive the engine instance
    for (int i = 0; i < nb_threads / 2; ++i) {
      instance.start([&mut, &data]() mutable {
        for (int j = 0; j < nb_iter; ++j) {
          mut.lock();
          data.push_back(1);
          // boson::sleep(2ms);
          mut.unlock();
        }
      });
      instance.start([&mut2, &data2]() mutable {
        for (int j = 0; j < nb_iter; ++j) {
          mut2.lock();
          data2.push_back(1);
          // boson::sleep(2ms);
          mut2.unlock();
        }
      });
    }
    t2 = high_resolution_clock::now();
  }
  auto t3 = high_resolution_clock::now();
  {
   std::vector<int> data;
   std::vector<int> data2;
  // Execute a routine communication through pipes
   boson::engine instance(nb_threads);
   std::mutex std_mut;
   std::mutex std_mut2;
   for (int i = 0; i < nb_threads/2; ++i) {
   instance.start([&data,&std_mut]() mutable {
   for (int j = 0; j < nb_iter; ++j) {
   std_mut.lock();
   data.push_back(1);
  //std::this_thread::sleep_for(2ms);
   std_mut.unlock();
  }
  });
   instance.start([&data2,&std_mut2]() mutable {
   for (int j = 0; j < nb_iter; ++j) {
   std_mut2.lock();
   data2.push_back(1);
  //std::this_thread::sleep_for(2ms);
   std_mut2.unlock();
  }
  });
  }
   t3 = high_resolution_clock::now();
  }
   auto t4 = high_resolution_clock::now();
   boson::debug::log("Pass 1: {}", duration_cast<milliseconds>(t2-t1).count());
   boson::debug::log("Pass 2: {}", duration_cast<milliseconds>(t3-t2).count());
   boson::debug::log("Pass 2: {}", duration_cast<milliseconds>(t4-t3).count());
}
