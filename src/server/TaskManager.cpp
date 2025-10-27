#include "server/TaskManager.hpp"

#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"
#include <cstdlib>
#include <thread>
#include <iostream>

static void fsm_waiting_task(Process &process, const ProgramConfig &config);
static void fsm_starting_task(Process &process);
static void fsm_running_task(Process &process);
static void fsm_exiting_task(Process &process);
static void fsm_stopped_task(Process &process);

int counter = 0;

TaskManager::TaskManager(std::unordered_map<std::string, std::vector<Process>>& process_pool, std::mutex& process_pool_mutex)
  : _process_pool(process_pool),
  _process_pool_mutex(process_pool_mutex),
  _worker_thread(std::thread(&TaskManager::work, this)),
  _stop_token(false)
{}

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
    for (auto& [name, processes] : _process_pool) {
      for (auto& process : processes) {
        fsm(process);
      }
    }
  }
  // TODO: Exit all processes cleanly
}

void TaskManager::fsm(Process &process) {
  const ProgramConfig& config = process.get_program_config();
  Process::status_t status;
  Process::State next_state;
  // std::cout << "prev state: " << process.get_previous_state() << std::endl;
  // std::cout << "curr state: " << process.get_state() << std::endl;
  switch (process.get_state()) {
    case Process::State::Waiting:
      std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << ">(Waiting)" << std::endl;
      fsm_waiting_task(process, config);
      if (!config.get_autostart()) {
        std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Waiting)>(Stopped)" << std::endl;
        next_state = Process::State::Stopped;
      } else if (config.get_starttime() != 0) {
        std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Waiting)>(Starting)" << std::endl;
        next_state = Process::State::Starting;
      } else {
        std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Waiting)>(Running)" << std::endl;
        next_state = Process::State::Running;
      }
      break;
    case Process::State::Starting:
      fsm_starting_task(process);
      next_state = Process::State::Starting;
      status = process.get_status();
      if (!status.running) {
        next_state = Process::State::Stopped;
      } else if (process.get_runtime() >= config.get_starttime()) {
        next_state = Process::State::Running;
      } else if (process.get_pending_command() == Process::Command::Restart || process.get_pending_command() == Process::Command::Stop) {
        next_state = Process::State::Exiting;
      }
      break;
    case Process::State::Running:
      fsm_running_task(process);
      next_state = Process::State::Running;
      status = process.get_status();
      if (!status.running) {
        std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Running)>(Stopped)" << std::endl;
        next_state = Process::State::Stopped;
      } else if (process.get_pending_command() == Process::Command::Restart || process.get_pending_command() == Process::Command::Stop) {
        std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Running)>(Exiting)" << std::endl;
        next_state = Process::State::Exiting;
      }
      break;
    case Process::State::Exiting:
      fsm_exiting_task(process);
      next_state = Process::State::Exiting;
      status = process.get_status();
      if (!status.running) {
        next_state = Process::State::Stopped;
      } else if (process.get_stoptime() >= config.get_stoptime()) {
        next_state = Process::State::Stopped;
      }
      break;
    case Process::State::Stopped:
      fsm_stopped_task(process);
      next_state = Process::State::Stopped;
      status = process.get_status();
      if (status.running) {
        break;
      } else if ((process.get_pending_command() == Process::Command::Start || process.get_pending_command() == Process::Command::Restart) ||
          process.get_num_retries() < config.get_startretries() ||
          process.check_autorestart()) {
        if (config.get_starttime() == 0) {
          std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Stopped)>(Running)" << std::endl;
          next_state = Process::State::Running;
        } else {
          std::cout << "[TaskManager] " << config.get_name() << ":" << process.get_pid() << " (Stopped)>(Starting)" << std::endl;
          next_state = Process::State::Starting;
        }
      }
      counter++;
      std::cout << "counter:" << counter << std::endl;
      if (counter > 3) {
        std::exit(0);
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

static void fsm_starting_task(Process &process) {
  if (process.get_state() != process.get_previous_state()) {
    if (process.get_pending_command() == Process::Command::Start ||
        process.get_pending_command() == Process::Command::Restart) {
      // If a Start/Restart command was issued, reset num_retries and clear the command
      process.set_num_retries(0);
      process.set_pending_command(Process::Command::None);
    }
    process.start();
  }
  process.update_status();
  if (!process.get_status().running) {
    // The process haven't run enough time to be considered successfully started
    process.set_num_retries(process.get_num_retries() + 1);
  }
}

static void fsm_running_task(Process &process) {
  if (process.get_state() != process.get_previous_state()) {
    if (process.get_pending_command() == Process::Command::Start ||
        process.get_pending_command() == Process::Command::Restart) {
      // If a Start/Restart command was issued, clear the command
      process.set_pending_command(Process::Command::None);
    }
    process.start();
  }
  process.update_status();
}

static void fsm_exiting_task(Process &process) {
  process.update_status();
}

static void fsm_stopped_task(Process &process) {
  if (process.get_previous_state() == Process::State::Exiting && process.get_status().running) {
    process.kill();
    process.update_status();
    return;
  }
  if (process.get_pending_command() == Process::Command::Stop) {
    process.set_pending_command(Process::Command::None);
  }
}

