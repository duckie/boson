
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <chrono>


#include "boson/stack.h"
#include "boson/fcontext.h"

using namespace std::chrono;
using namespace boson::stack;
using namespace boson::context;

static void foo( boson::context::transfer_t t_) {
    boson::context::transfer_t t = t_;
    while ( true) {
        t = boson::context::jump_fcontext( t.fctx, 0);
    }
}

size_t measure_time_fc() {
 		stack_context sctx = allocate<default_stack_traits>();
    fcontext_t ctx = make_fcontext(
            sctx.sp,
            sctx.size,
            foo);

    // cache warum-up
    transfer_t t = jump_fcontext( ctx, 0);

    auto start = high_resolution_clock::now();
    for ( std::size_t i = 0; i < 1000; ++i) {
        t = jump_fcontext( t.fctx, 0);
    }
    auto total = duration_cast<milliseconds>(high_resolution_clock::now() - start);
    //total -= overhead_clock(); // overhead of measurement
    total /= 1000;  // loops
    total /= 2;  // 2x jump_fcontext

    return total.count();
}


int main( int argc, char * argv[])
{
    try
    {
        auto res = measure_time_fc();
        std::cout << "fcontext_t: average of " << res << " nano seconds" << std::endl;

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
