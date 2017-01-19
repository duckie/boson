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

static constexpr int nb_iter = 2 * 1e6;
static constexpr int nb_threads = 4;

int main(int argc, char* argv[]) {
  boson::debug::logger_instance(&std::cout);

  std::vector<int> data(nb_iter,0);
  std::vector<int> data2(nb_iter,0);
  // Execute a routine communication through pipes
  boson::run(nb_threads, [&]() {
    using namespace boson;
    boson::mutex mut;   // if not shared, it must outlive the engine instance
    boson::mutex mut2;  // if not shared, it must outlive the engine instance

    for (int i = 0; i < nb_threads / 2; ++i) {
      boson::start([mut, i, &data]() mutable {
        for (int j = 0; j < nb_iter; ++j) {
          mut.lock();
          data[i] = i;
          mut.unlock();
        }
      });
      boson::start([mut2, i, &data2]() mutable {
        for (int j = 0; j < nb_iter; ++j) {
          mut2.lock();
          data2[j]=i;
          mut2.unlock();
        }
      });
    }
  });
}
