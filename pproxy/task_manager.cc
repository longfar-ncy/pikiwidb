/*
 * Copyright (c) 2023-present, OpenAtom Foundation, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "task_manager.h"

#include <algorithm>
#include <sstream>

Task::Status Task::TextToStatus(const std::string& input) {
  if (input == "pending") {
    return Task::pending;
  } else if (input == "completed") {
    return Task::completed;
  } else if (input == "deleted") {
    return Task::deleted;
  } else if (input == "recurring") {
    return Task::recurring;
  } else if (input == "waiting") {
    return Task::waiting;
  }

  return Task::pending;
}
std::string Task::StatusToText(Status s) {
  if (s == Task::pending) {
    return "pending";
  } else if (s == Task::completed) {
    return "completed";
  } else if (s == Task::deleted) {
    return "deleted";
  } else if (s == Task::recurring) {
    return "recurring";
  } else if (s == Task::waiting) {
    return "waiting";
  }
  return "pending";
}

TaskManager::TaskManager(std::shared_ptr<Threadpool> threadpool, size_t maxWorkers)
    : _threadpool(std::move(threadpool)), _maxWorkers(maxWorkers) {}

std::future<void> TaskManager::stop() {
  auto task = std::make_shared<std::packaged_task<void()>>([this] {
    std::unique_lock<std::mutex> guard(_mutex);
    bool isLast = _workerCount == 1;

    // Guarantee that the task finishes last.
    while (!isLast) {
      guard.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      guard.lock();
      isLast = _workerCount == 1;
    }
  });
  auto future = task->get_future();

  // Adding a new task and expecting the future guarantees that the last batch of tasks is being executed.
  auto functor = [task = std::move(task)]() mutable { (*task)(); };
  std::lock_guard<std::mutex> guard(_mutex);

  _stopped = true;
  _tasks.emplace(std::move(functor));
  this->processTasks();

  return future;
}

void TaskManager::addTask(std::function<void()> functor) {
  std::lock_guard<std::mutex> guard(_mutex);

  if (_stopped) {
    return;
  }
  _tasks.emplace(std::move(functor), Clock::now());
  this->processTasks();
}

void TaskManager::processTasks() {
  if (_tasks.empty() || _workerCount == _maxWorkers) {
    return;
  }
  auto task = std::move(_tasks.front());
  _tasks.pop();

  ++_workerCount;
  _threadpool->execute(std::move(task));
}