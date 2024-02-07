#pragma once

#include <chrono>

#include "fmt/core.h"

class TimerGuard {
 public:
  explicit TimerGuard(std::string&& name = "") : name_(name) { start_ = std::chrono::steady_clock::now(); }
  ~TimerGuard() {
    Stop();
    fmt::println("{} cost {}s", name_, std::chrono::duration<double>{end_ - start_}.count());
  }

 private:
  void Stop() { end_ = std::chrono::steady_clock::now(); }
  std::string name_;
  std::chrono::steady_clock::time_point start_;
  std::chrono::steady_clock::time_point end_;
};
