#include "server/TaskManager.hpp"

#include "server/Process.hpp"
#include "server/config/ProgramConfig.hpp"
#include <cstdlib>

TaskManager::TaskManager(std::unordered_map<std::string, std::vector<Process>>& process_pool, std::mutex& process_pool_mutex)
  : _process_pool(process_pool),
  _process_pool_mutex(process_pool_mutex)
{

}

static void fsm_waiting_task(Process &process, const ProgramConfig &config);
static void fsm_starting_task(Process &process);
static void fsm_running_task(Process &process);
static void fsm_exiting_task(Process &process);
static void fsm_stopped_task(Process &process);

void fsm(Process &process) {
  const ProgramConfig& config = process.get_program_config();
  Process::status_t status;
  switch (process.get_state()) {
    case Process::State::Waiting:
      fsm_waiting_task(process, config);
      if (!config.get_autostart()) {
        process.set_state(Process::State::Stopped);
      } else if (config.get_starttime() == 0) {
        process.set_state(Process::State::Starting);
      } else {
        process.set_state(Process::State::Running);
      }
      break;
    case Process::State::Starting:
      fsm_starting_task(process);
      status = process.get_status();
      if (!status.running) {
        process.set_state(Process::State::Stopped);
      } else if (process.get_runtime() >= config.get_starttime()) {
        process.set_state(Process::State::Running);
      } else if (process.get_pending_command() == Process::Command::Restart || process.get_pending_command() == Process::Command::Stop) {
        process.set_state(Process::State::Exiting);
      }
      break;
    case Process::State::Running:
      fsm_running_task(process);
      status = process.get_status();
      if (!status.running) {
        process.set_state(Process::State::Stopped);
      } else if (process.get_pending_command() == Process::Command::Restart || process.get_pending_command() == Process::Command::Stop) {
        process.set_state(Process::State::Exiting);
      }
      break;
    case Process::State::Exiting:
      fsm_exiting_task(process);
      status = process.get_status();
      if (!status.running) {
        process.set_state(Process::State::Stopped);
      } else if (process.get_stoptime() >= config.get_stoptime()) {
        process.set_state(Process::State::Stopped);
      }
      break;
    case Process::State::Stopped:
      fsm_stopped_task(process);
      status = process.get_status();
      if (status.running) {
        break;
      } else if ((process.get_pending_command() == Process::Command::Start || process.get_pending_command() == Process::Command::Restart) ||
          process.get_num_retries() < config.get_startretries() ||
          process.check_autorestart()) {
        if (config.get_starttime() == 0) {
          process.set_state(Process::State::Running);
        } else {
          process.set_state(Process::State::Starting);
        }
      }
      break;
  }
}

static void fsm_waiting_task(Process &process, const ProgramConfig &config) {
  if (config.get_autostart()) {
    process.start();
  }
}

static void fsm_starting_task(Process &process) {
  if (process.get_pid() == -1) {
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
  if (process.get_pid() == -1) {
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
  if (process.get_status().running) {
    process.kill();
    process.update_status();
    return;
  }
  if (process.get_pending_command() == Process::Command::Stop) {
    process.set_pending_command(Process::Command::None);
  }
}

