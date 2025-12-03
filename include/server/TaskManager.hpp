#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

#include "PollFds.hpp"
#include "Taskmaster.hpp"
#include "server/Process.hpp"

#include <mutex>
#include <sys/poll.h>
#include <thread>
#include <unordered_map>
#include <vector>

class TaskManager {
public:
  explicit TaskManager(
      std::unordered_map<std::string, std::vector<Process>> &process_pool,
      std::mutex &process_pool_mutex, PollFds &poll_fds, int wake_up_fd);
  ~TaskManager();

private:
  std::unordered_map<std::string, std::vector<Process>> &_process_pool;
  std::mutex &_process_pool_mutex;
  std::thread _worker_thread;
  std::atomic<bool> _stop_token;
  PollFds &_poll_fds;
  int _wake_up_fd;

  void work();
  void fsm(Process &process);

  void fsm_run_task(Process &process, const process_config_t &config);
  static void fsm_transit_state(Process &process,
                                const process_config_t &config);
  static void fsm_waiting_task(Process &process,
                               const process_config_t &config);
  void fsm_starting_task(Process &process, const process_config_t &config);
  static void fsm_running_task(Process &process);
  static void fsm_exiting_task(Process &process,
                               const process_config_t &config);
  void fsm_stopped_task(Process &process);
};

#endif // TASKMANAGER_HPP
