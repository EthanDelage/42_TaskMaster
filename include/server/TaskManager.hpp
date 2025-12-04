#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP

#include "PollFds.hpp"
#include "server/Process.hpp"
#include "server/ProcessPool.hpp"

#include <atomic>
#include <thread>

#define WAKE_UP_STRING "x"

class TaskManager {
public:
  TaskManager(ProcessPool &process_pool, PollFds &poll_fds);
  ~TaskManager();

  void start();
  void stop();
  bool is_thread_alive() const;

  void set_wake_up_fd(int wake_up_fd);

private:
  ProcessPool &_process_pool;
  std::thread _worker_thread;
  std::atomic<bool> _stop_token;
  PollFds &_poll_fds;
  int _wake_up_fd;

  void work();
  void fsm(Process &process);
  void exit_gracefully();

  void fsm_run_task(Process &process, const process_config_t &config);
  static void fsm_transit_state(Process &process,
                                const process_config_t &config);
  static void fsm_waiting_task();
  void fsm_starting_task(Process &process, const process_config_t &config);
  static void fsm_running_task(Process &process);
  static void fsm_exiting_task(Process &process,
                               const process_config_t &config);
  void fsm_stopped_task(Process &process);
  bool exit_process_gracefully(Process &process);
};

#endif // TASKMANAGER_HPP
