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
  std::ofstream file(filename);
  if (file) {
    std::string buffer;
    while(input >> buffer)
      file << buffer << std::flush;
  }
}
};

int main(int argc, char *argv[]) {
  boson::run(3, []() {
    boson::channel<std::string, 1> pipe;

    // Listen stdin
    boson::start_explicit(0, [](int in, auto output) -> void {
      char buffer[2048];
      ssize_t nread = 0;
      while(0 < (nread = ::read(in, &buffer, sizeof(buffer)))) {
        output << std::string(buffer,nread);
        std::cout << "iter" << std::endl;
      }
      output.close();
    }, 0, pipe);
 
    // Output in files
    writer functor;
    boson::start_explicit(1, functor, pipe, "file1.txt");
    boson::start_explicit(2, functor, pipe, "file2.txt");
  });
}
