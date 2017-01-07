Boson framework user manual
---------------------------

This documentation will walk you into each feature of the library.

# Getting started

*Warning*: as of January 2017, the `boson`framework only supports Linux.

## Third parties

`boson` uses a bunch of small third party libraries. Most of them are either header only or built directly into the `boson` library. All of their code is packaged within the `boson` repository. You dont have to download them and there is no submodule to retrieve.

## Build `boson`

To build the `boson` framework, you need `CMake` and a C++14 compiler.

```bash
git clone https://github.com/duckie/boson
mkdir build
cd build
cmake ..
make -j
```

`libboson` (the whole framework binary part), 'libfmt' ([awesome string formating tool](http://fmtlib.net))  are availables in `build/lib` as static libraries. It is *highly recommended* not to use `boson` as a shared library.

## Quick tutorial

We'll start with a very simple program and explain each part of it.

```c++
#include "boson/boson.h"
#include "boson/channel.h"
#include <iostream>
#include <string>

int main() {
  boson::run(1, []() {
    using namespace boson;

    channel<std::string, 2> msg_chan;

    start([](auto chan) -> void {
      std::string received_message;
      while (chan >> received_message)
        std::cout << received_message << "\n";
    }, msg_chan);

    start([](auto chan) -> void {
      chan << "Hello world !";
      chan.close();
    }, msg_chan);
  });
  return 0;
}
```





