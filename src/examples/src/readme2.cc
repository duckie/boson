#include "stdio.h"
#include "boson/boson.h"
#include "boson/channel.h"
#include <string>
#include <fstream>
#include <iostream>

using namespace std::literals;

struct writer {
template <class Channel>
void operator()(Channel input, char const* filename) const {
  auto file = std::ofstream(filename);
  if (file) {
    std::string buffer;
    for(;;) {
        input >> buffer;
        file << buffer << std::flush;
    }
  }
}
};

int main(int argc, char *argv[]) {
  boson::run(3, []() {
    boson::channel<std::string, 3> pipe;

    // Listen stdin
    boson::start_explicit(0, [](int in, auto output) -> void {
      char buffer[2048];
      while(0 < boson::read(in, &buffer, sizeof(buffer))) {
        output << std::string(buffer);
      }
    }, 0, pipe);

    // Output in files
    writer functor;
    boson::start_explicit(1, functor, pipe, "file1.txt");
    boson::start_explicit(2, functor, pipe, "file2.txt");
  });
}
