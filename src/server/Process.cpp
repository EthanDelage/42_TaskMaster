#include "server/Process.hpp"

Process::Process(std::string cmd): _cmd(std::move(cmd)) {}
