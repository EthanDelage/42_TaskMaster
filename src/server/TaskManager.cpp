#include "server/TaskManager.hpp"

#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>

static void fsm_run_task(Process &process, const ProgramConfig &config);
static void fsm_transit_state(Process &process, const ProgramConfig &config);
static void fsm_waiting_task(Process &process, const ProgramConfig &config);
static void fsm_starting_task(Process &process, const ProgramConfig &config);
static void fsm_running_task(Process &process);
static void fsm_exiting_task(Process &process, const ProgramConfig &config);
static void fsm_stopped_task(Process &process);

TaskManager::TaskManager(
    std::unordered_map<std::string, std::vector<Process>> &process_pool,
    std::mutex &process_pool_mutex)
    : _process_pool(process_pool),
      _process_pool_mutex(process_pool_mutex),
      _worker_thread(std::thread(&TaskManager::work, this)),
      _stop_token(false) {}

TaskManager::~TaskManager() {
  _stop_token = true;
  if (_worker_thread.joinable()) {
    _worker_thread.join();
  } else {
    std::cerr << "Worker thread is not joinable which is a bit weird\n";
  }
}

void TaskManager::work() {
  while (!_stop_token) {
    std::lock_guard<std::mutex> lock(_process_pool_mutex);
    for (auto &[name, processes] : _process_pool) {
      for (auto &process : processes) {
        // std::cout << "[TaskManager] fsm(" <<
        // process.get_program_config().get_name() << ")" << std::endl;
        fsm(process);
      }
    }
  }
  // TODO: Exit all processes cleanly
}

void TaskManager::fsm(Process &process) {
  const ProgramConfig &config = process.get_program_config();
  fsm_run_task(process, config);
  fsm_transit_state(process, config);
}

static void fsm_run_task(Process &process, const ProgramConfig &config) {
  switch (process.get_state()) {
  case Process::State::Waiting:
    fsm_waiting_task(process, config);
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

static void fsm_transit_state(Process &process, const ProgramConfig &config) {
  Process::status_t status;
  Process::State next_state;
  switch (process.get_state()) {
  case Process::State::Waiting:
    if (!config.get_autostart()) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Waiting)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    } else {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Waiting)>(Starting)" << std::endl;
      next_state = Process::State::Starting;
    }
    break;
  case Process::State::Starting:
    next_state = Process::State::Starting;
    status = process.get_status();
    if (config.get_starttime() == 0) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Starting)>(Running)" << std::endl;
      next_state = Process::State::Running;
    } else if (!status.running) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Starting)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    } else if (process.get_runtime() >= config.get_starttime()) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Starting)>(Running)" << std::endl;
      next_state = Process::State::Running;
    } else if (process.get_pending_command() == Process::Command::Restart ||
               process.get_pending_command() == Process::Command::Stop) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Starting)>(Exiting)" << std::endl;
      next_state = Process::State::Exiting;
    }
    break;
  case Process::State::Running:
    next_state = Process::State::Running;
    status = process.get_status();
    if (!status.running) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Running)>(Stopped)" << std::endl;
      next_state = Process::State::Stopped;
    } else if (process.get_pending_command() == Process::Command::Restart ||
               process.get_pending_command() == Process::Command::Stop) {
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Running)>(Exiting)" << std::endl;
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
    status = process.get_status();
    if ((process.get_pending_command() == Process::Command::Start ||
         process.get_pending_command() == Process::Command::Restart) ||
        (process.get_previous_state() == Process::State::Running &&
         process.check_autorestart()) || // Process was successfully started and
                                         // needs autorestart
        (process.get_previous_state() == Process::State::Starting &&
         process.get_num_retries() <=
             config.get_startretries())) { // Process was unsuccessfully started
                                           // and num_retries <= startretries
      std::cout << "[TaskManager] " << config.get_name() << ":"
                << " (Stopped)>(Starting)" << std::endl;
      next_state = Process::State::Starting;
    }
    break;
  }
  process.set_previous_state(process.get_state());
  process.set_state(next_state);
}

static void fsm_waiting_task(Process &process, const ProgramConfig &config) {
  if (config.get_autostart()) {
    process.start();
  }
}

static void fsm_starting_task(Process &process, const ProgramConfig &config) {
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
  if (config.get_starttime() !=
      0) { // Wait the process only if starttime is set
    process.update_status();
  }
  if (!process.get_status().running) {
    // The process haven't run enough time to be considered successfully started
    process.set_num_retries(process.get_num_retries() + 1);
  }
}

static void fsm_running_task(Process &process) { process.update_status(); }

static void fsm_exiting_task(Process &process, const ProgramConfig &config) {
  process.update_status();
  if (process.get_stoptime() >= config.get_stoptime() &&
      process.get_status().running) {
    process.kill();
    process.update_status();
  }
}

static void fsm_stopped_task(Process &process) {
  if (process.get_pending_command() == Process::Command::Stop) {
    process.set_pending_command(Process::Command::None);
  }
}
