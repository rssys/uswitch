#include <stdint.h>
#include <chrono>

std::chrono::steady_clock::time_point startTimer();
std::chrono::steady_clock::time_point stopTimer();
int64_t diffInNanoseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end);
