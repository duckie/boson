The Boson framework
====================

The Boson Framework is a C++ library to write concurrent software in C++. The framework inspires freely from what the Go language did to address complexity in concurrent programming. The Boson Framework implements its own versions of three main Go features:

* The Go routines
* The net poller
* The channels

This project is currently a proof of concept. New features and performance improvements are to be expected in the near future.

The Boson Framework only supports Linux on x86-64 platforms for now, but Windows and Mac OS are in the roadmap.

### What's the point ?

The main point is to simplify writing concurrent applications by removing the asynchronous logic from the developer's mind. The developer only uses blocking calls, and the whole logic is written sequentially. Under the hood, blocking calls are not blocking from the OS standpoint, since the thread is still used to execute some tasks. This is different from using multiple thread because :
- Routines (or fibers) are much more lighter than threads to launch and to schedule
- Interruption points are known by the developer, whereas they are not when scheduling threads.

### What about `boost::fiber` ?

There are technical reasons that might make `boson` a better match for your use case:
- *Ease of use*: Using `boost::fiber` combined with `boost::asio` requires a decent experience of *C++* to be used efficiently. The knowledge curve is steep.
- *System interface*: the `boson` framework mimics the common system calls. The reason for that is to make synchronous code easily portable into `boson`: you just have to route the system calls to those of `boson`, and make you calls from within a routine. Database drivers are much more easier to write this way. This is also a way to create a familiar ground for *C* and *Go* users.
- *Select statement*: The `select_*` statement is the key feature of the framework. It is more versatile than the *Go* one, and does not exist in `boost::fiber` (as of time of writing). The `select` statement allows for programming constructs that are hard to reproduce without it.

That said, it is not excluded to use `boost::fiber` as a backend for `boson` in the future if it proves efficient to do so.

## Quick overview

The goal of the framework is to use light routines instead of threads. In Go, these are called Goroutines. In the C++ world, it is known as fibers. In the boson framework, we just call it routines.

### Launch an instance

This snippet just launches a simple routine that exits immediately.

```C++
boson::run(1 /* number of thread */, []() {
  std::cout << "Hello world" << std::endl;
});
```

### System calls

The boson framework provides its versions of system calls that are scheduled away for efficiency with an event loop. Current available syscalls are `sleep`, `read`, `write`, `accept`, `recv` and `send`. See [an example](./src/examples/src/socket_server.cc).

This snippet launches two routines doing different jobs, in a single thread.

```C++
void timer(int out, bool& stopper) {
  while(!stopper) {
    boson::sleep(1000ms);
    boson::write(out,"Tick !\n",7);
  }
  boson::write(out,"Stop !\n",7);
}

void user_input(int in, bool& stopper) {
  char buffer[1];
  boson::read(in, &buffer, sizeof(buffer));
  stopper = true;
}

int main(int argc, char *argv[]) {
  bool stopper = false;
  boson::run(1, [&stopper]() {
    boson::start(timer, 1, stopper);
    boson::start(user_input, 0, stopper);
  });
}
```

This executable prints `Tick !` every second and quits if the user enters anything on the standard input. We dont have to manage concurrency on the `stopper` variable since we know only one thread uses it. Plus, we know at which points the routines might be interrupted.

## Channels

The boson framework implements channels similar to Go ones. Channels are used to communicate between routines. They can be used over multiple threads.

This snippet listens to the standard input in one thread and writes to two different files in two other threads. A channel is used to communicate. This also demonstrates the use of a generic lambda and a generic functor. Boson system calls are not used on the files because files on disk cannot be polled for events (not allowed by the Linux kernel).

```C++
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
```

Channels must be transfered by copy. Generic lambdas, when used as a routine seed, must explicitely state that they return `void`. The why will be explained in detail in further documentation. Threads are assigned to routines in a round-robin fashion. The thread id can be explicitely given when starting a routine.

```C++
boson::start_explicit(0, [](int in, auto output) -> void {...}, 0, pipe);
boson::start_explicit(1, functor, pipe, "file1.txt");
boson::start_explicit(2, functor, pipe, "file2.txt");
```

See [an example](./src/examples/src/channel_loop.cc).

## The select statement

The select statement is similar to the Go one, but with a nice twist : it can be used with a mix of channels and syscalls. 

```C++
// Create a pipe
int pipe_fds[2];
::pipe(pipe_fds);

boson::run(1, [&]() {
using namespace boson;

// Create channel
channel<int, 3> chan;

// Start a producer
start([](int out, auto chan) -> void {
    int data = 1;
    boson::write(out, &data, sizeof(data));
    chan << data;
}, pipe_fds[1], chan);

// Start a consumer
start([](int in, auto chan) -> void {
    int buffer = 0;
    bool stop = false;
    while (!stop) {
      select_any(  //
          event_read(in, &buffer, sizeof(buffer),
                     [](ssize_t rc) {  //
                       std::cout << "Got data from the pipe \n";
                     }),
          event_read(chan, buffer,
                     [](bool) {  //
                       std::cout << "Got data from the channel \n";
                     }),
          event_timer(100ms,
                      [&stop]() {  //
                        std::cout << "Nobody loves me anymore :(\n";
                        stop = true;
                      }));
    }
},  pipe_fds[0], chan);
});
```

`select_any` can return a value :


```C++
int result = select_any(                                                    //
    event_read(in, &buffer, sizeof(buffer), [](ssize_t rc) { return 1; }),  //
    event_read(chan, buffer, [](bool) { return 2; }),                       //
    event_timer(100ms, []() { return 3; }));                                //
switch(result) {
  case 1:
    std::cout << "Got data from the pipe \n";
    break;
  case 2:
    std::cout << "Got data from the channel \n";
    break;
  default:
    std::cout << "Nobody loves me anymore :(\n";
    stop = true;
    break;
}
```

See [an example](./src/examples/src/chat_server.cc).

## See other examples

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
