#include "server/TaskManager.hpp"

#include "common/Logger.hpp"
#include "common/socket/Socket.hpp"
#include "server/ConfigParser.hpp"
#include "server/Process.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>

TaskManager::TaskManager(ProcessPool &process_pool, PollFds &poll_fds)
    : _process_pool(process_pool),
      _stop_token(true),
      _poll_fds(poll_fds),
      _wake_up_fd(-1) {
  std::cout << "TaskManager::TaskManager()" << std::endl;
}

TaskManager::~TaskManager() {
  _stop_token = true;
  if (_worker_thread.joinable()) {
    _worker_thread.join();
  } else {
    Logger::get_instance().error(
        "Worker thread is not joinable which is a bit weird");
  }
}

void TaskManager::start() {
  if (_wake_up_fd == -1) {
    throw std::runtime_error("TaskManager::start: wake up fd not set");
  }
  _stop_token = false;
  _worker_thread = std::thread(&TaskManager::work, this);
}

void TaskManager::stop() { _stop_token = true; }

bool TaskManager::is_thread_alive() const { return (!_stop_token); }

void TaskManager::set_wake_up_fd(int wake_up_fd) { _wake_up_fd = wake_up_fd; }

void TaskManager::work() {
  std::cout << "TaskManager::work()" << std::endl;
  try {
    while (!_stop_token) {
      std::lock_guard lock(_process_pool.get_mutex());
      for (auto &[_, process_group] : _process_pool) {
        for (auto &process : process_group) {
          fsm(process);
        }
      }
    }
  } catch (std::exception &e) {
    _stop_token = true;
    std::cout << e.what() << std::endl;
  }
  exit_gracefully();
}

void TaskManager::exit_gracefully() {
  bool flag;
  do {
    flag = false;
    std::lock_guard lock(_process_pool.get_mutex());
    for (auto &[_, process_group] : _process_pool) {
      for (auto &process : process_group) {
        if (!exit_process_gracefully(process)) {
          // The process did not exit yet, so we stay in the loop
          flag = true;
        }
      }
    }
  } while (flag);
  Socket::write(_wake_up_fd, WAKE_UP_STRING);
  std::cout << "[TaskManager] exited gracefully" << std::endl;
}

/*
 * @return true if the process exited, false otherwise
 */
bool TaskManager::exit_process_gracefully(Process &process) {
  if (process.get_pid() == -1) {
    process.set_state(Process::State::Stopped);
  }
  switch (process.get_state()) {
  case Process::State::Waiting:
    process.set_state(Process::State::Stopped);
    break;
  case Process::State::Starting:
  case Process::State::Running:
    process.set_state(Process::State::Exiting);
    break;
  case Process::State::Exiting:
    try {
      process.update_status();
    } catch (std::exception &e) {
      std::cerr << "Could not update " << process.get_process_config().name
                << " status" << std::endl;
      std::cerr << e.what() << std::endl;
      break;
    }
    if (process.get_state() != process.get_previous_state()) {
      std::cout << "[TaskManager] Quitting "
                << process.get_process_config().name << std::endl;
      try {
        process.stop(process.get_process_config().stopsignal);
      } catch (std::exception &e) {
        std::cerr << "Could not stop " << process.get_process_config().name
                  << std::endl;
        std::cerr << e.what() << std::endl;
      }
    }
    if (process.get_stoptime() >= process.get_process_config().stoptime &&
        process.get_status().running) {
      if (!process.get_status().killed) {
        std::cout << "[TaskManager] Killing "
                  << process.get_process_config().name << std::endl;
        try {
          process.kill();
        } catch (std::exception &e) {
          std::cerr << "Could not kill " << process.get_process_config().name
                    << std::endl;
          std::cerr << e.what() << std::endl;
        }
      }
    }
    if (!process.get_status().running) {
      process.set_state(Process::State::Stopped);
    }
    process.set_previous_state(Process::State::Exiting);
    break;
  case Process::State::Stopped:
    if (process.get_state() != process.get_previous_state()) {
      std::cout << "[TaskManager] Stopped " << process.get_process_config().name
                << std::endl;
    }
    process.set_previous_state(Process::State::Stopped);
    return true;
  }
  return false;
}

void TaskManager::fsm(Process &process) {
  const process_config_t &config = process.get_process_config();
  fsm_run_task(process, config);
  fsm_transit_state(process, config);
}

void TaskManager::fsm_run_task(Process &process,
                               const process_config_t &config) {
  switch (process.get_state()) {
  case Process::State::Waiting:
    fsm_waiting_task();
    break;
  case Process::State::Starting:
    fsm_starting_task(process, config);
    break;
  case Process::State::Running:
    fsm_running_task(process);
    break;
  case Process::State::Exiting:
    fsm_exiting_task(process, config);
    break;
  case Process::State::Stopped:
    fsm_stopped_task(process);
    break;
  }
}

void TaskManager::fsm_transit_state(Process &process,
                                    const process_config_t &config) {
  Process::status_t status;
  Process::State next_state;
  switch (process.get_state()) {
  case Process::State::Waiting:
    if (!config.autostart) {
      next_state = Process::State::Stopped;
    } else {
      next_state = Process::State::Starting;
    }
    break;
  case Process::State::Starting:
    next_state = Process::State::Starting;
    status = process.get_status();
    if (config.starttime == 0) {
      next_state = Process::State::Running;
    } else if (!status.running) {
      next_state = Process::State::Stopped;
    } else if (process.get_runtime() >= config.starttime) {
      next_state = Process::State::Running;
    } else if (process.get_pending_command() == Process::Command::Restart ||
               process.get_pending_command() == Process::Command::Stop) {
      next_state = Process::State::Exiting;
               }
    break;
  case Process::State::Running:
    next_state = Process::State::Running;
    status = process.get_status();
    if (!status.running) {
      next_state = Process::State::Stopped;
    } else if (process.get_pending_command() == Process::Command::Restart ||
               process.get_pending_command() == Process::Command::Stop) {
      next_state = Process::State::Exiting;
    }
    break;
  case Process::State::Exiting:
    next_state = Process::State::Exiting;
    status = process.get_status();
    if (!status.running) {
      next_state = Process::State::Stopped;
    }
    break;
  case Process::State::Stopped:
    next_state = Process::State::Stopped;
    if ((process.get_pending_command() == Process::Command::Start ||
         process.get_pending_command() == Process::Command::Restart) ||
        (process.get_previous_state() == Process::State::Running &&
         process.check_autorestart()) || // Process was successfully started and
                                         // needs autorestart
        (process.get_previous_state() == Process::State::Starting &&
         process.get_num_retries() <=
             config.startretries)) { // Process was unsuccessfully started
                                     // and num_retries <= startretries
      next_state = Process::State::Starting;
    }
    break;
  }
  if (next_state != process.get_state()) {
    Logger::get_instance().debug(process.str() + ": " +
      process_state_str(process.get_state()) + ">" + process_state_str(next_state));
  }
  process.set_previous_state(process.get_state());
  process.set_state(next_state);
}

void TaskManager::fsm_waiting_task(void) {}

void TaskManager::fsm_starting_task(Process &process,
                                    const process_config_t &config) {
  if (process.get_state() != process.get_previous_state()) {
    if (process.get_pending_command() == Process::Command::Start ||
        process.get_pending_command() == Process::Command::Restart) {
      // If a Start/Restart command was issued, reset num_retries and clear the
      // command
      process.set_num_retries(0);
      process.set_pending_command(Process::Command::None);
    }
    process.start();
    _poll_fds.add_poll_fd({process.get_stdout_pipe()[PIPE_READ], POLLIN, 0},
                          {PollFds::FdType::Process});
    _poll_fds.add_poll_fd({process.get_stderr_pipe()[PIPE_READ], POLLIN, 0},
                          {PollFds::FdType::Process});
    Socket::write(_wake_up_fd, WAKE_UP_STRING);
  }
  if (config.starttime != 0) { // Wait the process only if starttime is set
    process.update_status();
  }
  if (!process.get_status().running) {
    // The process haven't run enough time to be considered successfully started
    process.set_num_retries(process.get_num_retries() + 1);
  }
}

void TaskManager::fsm_running_task(Process &process) {
  process.update_status();
}

void TaskManager::fsm_exiting_task(Process &process,
                                   const process_config_t &config) {
  process.update_status();
  if (process.get_state() != process.get_previous_state()) {
    process.stop(config.stopsignal);
    return;
  }
  if (process.get_stoptime() >= config.stoptime &&
      process.get_status().running) {
    if (!process.get_status().killed) {
      process.kill();
    }
  }
}

void TaskManager::fsm_stopped_task(Process &process) {
  if (process.get_state() != process.get_previous_state()) {
    _poll_fds.remove_poll_fd(process.get_stdout_pipe()[PIPE_READ]);
    _poll_fds.remove_poll_fd(process.get_stderr_pipe()[PIPE_READ]);
    Socket::write(_wake_up_fd, "x");
    process.close_outputs();
  }
  if (process.get_pending_command() == Process::Command::Restart) {
    return;
  }
  if (process.get_previous_state() != Process::State::Stopped ||
      process.get_pending_command() == Process::Command::Stop) {
    process.set_pending_command(Process::Command::None);
  }
}

