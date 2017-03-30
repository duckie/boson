#ifndef BOSON_EVENTLOOP_H_
#define BOSON_EVENTLOOP_H_
#pragma once

#include <cstdint>
#include <memory>
#include <tuple>

namespace boson {
using event_status = int;
enum class loop_end_reason { max_iter_reached, timed_out, error_occured };
};

#endif  // BOSON_EVENTLOOP_H_
