#include "timing.h"
#include <chrono>
using namespace std::chrono;

steady_clock::time_point startTimer() {
  return steady_clock::now();
}

steady_clock::time_point stopTimer() {
  return steady_clock::now();
}

int64_t diffInNanoseconds(steady_clock::time_point start, steady_clock::time_point end) {
  return duration_cast<nanoseconds>(end-start).count();
}
