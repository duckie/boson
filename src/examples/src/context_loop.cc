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

static constexpr int nb_iter = 1e5;
static constexpr int nb_threads = 8;
static constexpr int nb_tasks= nb_threads * 64;

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);

  auto t1 = high_resolution_clock::now();
  {
    //std::vector<int> data;
    //std::vector<int> data2;
    //// Execute a routine communication through pipes
    //std::mutex mut;
    //std::mutex mut2;
    //boson::engine instance(8);
    //for (int i = 0; i < nb_threads / 2; ++i) {
      //instance.start([&mut, &data]() mutable {
        //for (int j = 0; j < nb_iter; ++j) {
          //mut.lock();
          //data.push_back(1);
          ////// std::this_thread::sleep_for(2ms);
          //mut.unlock();
        //}
      //});
      //instance.start([&mut2, &data2]() mutable {
        //for (int j = 0; j < nb_iter; ++j) {
          //mut2.lock();
          //data2.push_back(1);
          //// boson::sleep(2ms);
          //mut2.unlock();
        //}
      //});
    //}
    t1 = high_resolution_clock::now();
  }
  auto t2b = high_resolution_clock::now();
  {
    std::vector<int> data;
    std::vector<int> data2;
    // Execute a routine communication through pipes
    boson::run(nb_threads, [&]() {
      using namespace boson;
      boson::mutex mut;   // if not shared, it must outlive the engine instance
      boson::mutex mut2;  // if not shared, it must outlive the engine instance

      for (int i = 0; i < nb_tasks/2; ++i) {
        boson::start([mut, &data]() mutable {
          for (int j = 0; j < nb_iter; ++j) {
            //mut.lock();
            //data.push_back(1);
            ////boson::sleep(2ms);
            //mut.unlock();
            boson::yield();
          }
        });
        boson::start([mut2, &data2]() mutable {
          for (int j = 0; j < nb_iter; ++j) {
            //mut2.lock();
            //data2.push_back(1);
            ////boson::sleep(2ms);
            //mut2.unlock();
            boson::yield();
          }
        });
      }
      t2b = high_resolution_clock::now();
    });
  }
  auto t2e = high_resolution_clock::now();
  auto t3b = t2e;
  {
    //std::vector<int> data;
    //std::vector<int> data2;
    //std::mutex std_mut;
    //std::mutex std_mut2;
    //// Execute a routine communication through pipes
    //boson::run(nb_tasks, [&]() mutable {
      //using namespace boson;
      //for (int i = 0; i < nb_tasks/2; ++i) {
        //start([&data, &std_mut]() mutable {
          //for (int j = 0; j < nb_iter; ++j) {
            ////std_mut.lock();
            ////data.push_back(1);
            //////std::this_thread::sleep_for(2ms);
            ////std_mut.unlock();
            //std::this_thread::yield();
          //}
        //});
        //start([&data2, &std_mut2]() mutable {
          //for (int j = 0; j < nb_iter; ++j) {
            ////std_mut2.lock();
            ////data2.push_back(1);
            //////std::this_thread::sleep_for(2ms);
            ////std_mut2.unlock();
            //std::this_thread::yield();
          //}
        //});
      //}
      //t3b = high_resolution_clock::now();
    //});
  }
  auto t3e = high_resolution_clock::now();
  std::cout << fmt::format("Pass 2: {}\n", duration_cast<milliseconds>(t2e - t2b).count())
            << fmt::format("Pass 3: {}\n", duration_cast<milliseconds>(t3e - t3b).count());
}
