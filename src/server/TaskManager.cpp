#include "server/TaskManager.hpp"

#include "server/ConfigParser.hpp"
#include "server/Process.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>

static void fsm_run_task(Process &process, const process_config_t &config);
static void fsm_transit_state(Process &process, const process_config_t &config);
static void fsm_waiting_task(void);
static void fsm_starting_task(Process &process, const process_config_t &config);
static void fsm_running_task(Process &process);
static void fsm_exiting_task(Process &process, const process_config_t &config);
static void fsm_stopped_task(Process &process);

TaskManager::TaskManager(ProcessPool &process_pool)
    : _process_pool(process_pool),
      _worker_thread(std::thread(&TaskManager::work, this)),
      _stop_token(false) {
  std::cout << "TaskManager::TaskManager()" << std::endl;
}

TaskManager::~TaskManager() {
  _stop_token = true;
  if (_worker_thread.joinable()) {
    _worker_thread.join();
  } else {
    std::cerr << "Worker thread is not joinable which is a bit weird\n";
  }
}

void TaskManager::work() {
  std::cout << "TaskManager::work()" << std::endl;
  while (!_stop_token) {
    std::lock_guard lock(_process_pool.get_mutex());
    for (auto &[_, process_group] : _process_pool) {
      for (auto &process : process_group) {
        fsm(process);
      }
    }
  }
  // TODO: Exit all processes cleanly
}

void TaskManager::fsm(Process &process) {
  const process_config_t &config = process.get_process_config();
  fsm_run_task(process, config);
  fsm_transit_state(process, config);
}

static void fsm_run_task(Process &process, const process_config_t &config) {
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

static void fsm_transit_state(Process &process,
                              const process_config_t &config) {
  Process::status_t status;
  Process::State next_state;
  switch (process.get_state()) {
  case Process::State::Waiting:
    if (!config.autostart) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Waiting)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    } else {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Waiting)>(Starting)" << std::endl;
      next_state = Process::State::Starting;
    }
    break;
  case Process::State::Starting:
    next_state = Process::State::Starting;
    status = process.get_status();
    if (config.starttime == 0) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Starting)>(Running)" << std::endl;
      next_state = Process::State::Running;
    } else if (!status.running) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Starting)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    } else if (process.get_runtime() >= config.starttime) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Starting)>(Running)" << std::endl;
      next_state = Process::State::Running;
    } else if (process.get_pending_command() == Process::Command::Restart ||
               process.get_pending_command() == Process::Command::Stop) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Starting)>(Exiting)" << std::endl;
      next_state = Process::State::Exiting;
    }
    break;
  case Process::State::Running:
    next_state = Process::State::Running;
    status = process.get_status();
    if (!status.running) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Running)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    } else if (process.get_pending_command() == Process::Command::Restart ||
               process.get_pending_command() == Process::Command::Stop) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Running)>(Exiting)" << std::endl;
      next_state = Process::State::Exiting;
    }
    break;
  case Process::State::Exiting:
    next_state = Process::State::Exiting;
    status = process.get_status();
    if (!status.running) {
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Exiting)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    }
    break;
  case Process::State::Stopped:
    next_state = Process::State::Stopped;
    status = process.get_status();
    if ((process.get_pending_command() == Process::Command::Start ||
         process.get_pending_command() == Process::Command::Restart) ||
        (process.get_previous_state() == Process::State::Running &&
         process.check_autorestart()) || // Process was successfully started and
                                         // needs autorestart
        (process.get_previous_state() == Process::State::Starting &&
         process.get_num_retries() <=
             config.startretries)) { // Process was unsuccessfully started
                                     // and num_retries <= startretries
      std::cout << "[TaskManager] " << config.name << ":"
                << " (Stopped)>(Starting)" << std::endl;
      next_state = Process::State::Starting;
    }
    break;
  }
  process.set_previous_state(process.get_state());
  process.set_state(next_state);
}

static void fsm_waiting_task(void) {}

static void fsm_starting_task(Process &process,
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
  }
  if (config.starttime != 0) { // Wait the process only if starttime is set
    process.update_status();
  }
  if (!process.get_status().running) {
    // The process haven't run enough time to be considered successfully started
    process.set_num_retries(process.get_num_retries() + 1);
  }
}

static void fsm_running_task(Process &process) { process.update_status(); }

static void fsm_exiting_task(Process &process, const process_config_t &config) {
  process.update_status();
  if (process.get_state() != process.get_previous_state()) {
    process.stop(config.stopsignal);
    return;
  } else if (process.get_stoptime() >= config.stoptime &&
             process.get_status().running) {
    if (!process.get_status().killed) {
      process.kill();
    }
  }
}

static void fsm_stopped_task(Process &process) {
  if (process.get_pending_command() == Process::Command::Restart) {
    return;
  }
  if (process.get_previous_state() != Process::State::Stopped ||
      process.get_pending_command() == Process::Command::Stop) {
    process.set_pending_command(Process::Command::None);
  }
}
