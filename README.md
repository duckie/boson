The Boson framework
====================

The Boson Framework is a C++ library to write concurrent software in C++. The framework inspires freely from what the Go language did to address complexity in concurrent programming. The Boson Framework implements its own versions of three main Go features:

* The Go routines
* The net poller
* The channels

This project is currently a proof of concept. New features and performance improvements are to be expected in the near future.

The Boson Framework only supports Linux on x86-64 platforms for now, but Windows and Mac OS are in the roadmap.

## Quick overview

The goal of the framework is to use ligh routines instead of threads. In Go, these are called Goroutines. In the C++ world, it is known as fibers. In the boson framework, we just call it routines.

### Launch an instance

This snippet just launches a simple routine that exits immediately.

```C++
boson::run(1 /* number of thread */, []() {
  std::cout << "Hello world" << std::endl;
});
```

### Two routines in one thread

This snippet launches two routines:

```C++
boson::run(1 /* number of thread */, []() {
  boson::start([]() {
    for (int i=0; i < 10; ++i) {
      std::cout << "I am routine A." << std::endl;
      boson::yield();  // Give back control to the scheduler
    }
  });
  boson::start([]() {
    for (int i=0; i < 10; ++i) {
      std::cout << "I am routine B." << std::endl;
      boson::yield();  // Give back control to the scheduler
    }
  });
});
```

## System calls

The boson framework provides its versions of system calls that are scheduled away for efficiency with an event loop. Current available syscalls are `sleep`, `read`, `write`, `accept`, `recv` and `send`. See [an example](./src/examples/src/socket_server.cc).

## Channels

The boson framework implements channels similar to Go ones. See [an example](./src/examples/src/channel_loop.cc).

## See examples

Head to the [examples](./src/examples/src) to see more code.


## How to build

The Boson framework needs a modern C++ compiler to be used.

```bash
git clone https://github.com/duckie/boson
mkdir -p boson/build
cd boson/build
cmake ../
make -j
```

Examples binaries will be built in `boson/build/bin`.
