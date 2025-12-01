#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

#include "server/Process.hpp"
#include "server/ProcessPool.hpp"

#include <atomic>
#include <thread>

class TaskManager {
public:
  explicit TaskManager(ProcessPool& process_pool);
  ~TaskManager();

private:
  ProcessPool& _process_pool;
  std::thread _worker_thread;
  std::atomic<bool> _stop_token;

  void work();
  void fsm(Process &process);
};

#endif // TASKMANAGER_HPP
