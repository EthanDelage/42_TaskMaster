#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

#include "server/Process.hpp"

#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>



class TaskManager {
public:
  explicit TaskManager(std::unordered_map<std::string, std::vector<Process>>& process_pool, std::mutex& process_pool_mutex);

  void spawn();

private:
  std::unordered_map<std::string, std::vector<Process>>& _process_pool;
  std::mutex& _process_pool_mutex;
  std::thread _worker_thread;

  void work();
  void fsm();
};

#endif // TASKMANAGER_HPP
