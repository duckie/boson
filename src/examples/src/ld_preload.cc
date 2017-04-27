#include "stdio.h"
#include "boson/boson.h"
#include "boson/channel.h"
#include <string>
#include <fstream>
#include <iostream>

int (*BosonOpenShim_fp)(const char*, int) = &boson::open;
boson::internal::thread*& (*BosonCurrentThread_fp)() = &boson::internal::current_thread;

int main(int argc, char *argv[]) {
  boson::run(1, []() {
    auto fp = open("/tmp/robert.txt", O_RDWR);
    ::close(fp);
  });
}
