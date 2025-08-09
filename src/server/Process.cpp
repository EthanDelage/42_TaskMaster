#include "server/Process.hpp"

#include "common/utils.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>

extern char **environ; // envp

Process::Process(std::string  cmd):_cmd(std::move(cmd)), _cmd_path(get_cmd_path(_cmd)), _pid(-1) {}

int Process::start() {
    _pid = fork();
    if (_pid == -1) {
        perror("fork");
        return -1;
    }
    if (_pid > 0) {
        return 0;
    }
    char *argv[] = {
        const_cast<char*>(_cmd_path.c_str()),
        nullptr
    };
    if (execve(_cmd_path.c_str(), argv, environ) == -1) {
        perror("execve");
        return -1;
    }
    return 0;
}

int Process::stop(const int sig) {
    if (kill(_pid, sig) == -1) {
        perror("kill");
        return -1;
    }
    if (wait(nullptr) == -1) {
        perror("wait");
        return -1;
    }
    _pid = -1;
    return 0;
}

int Process::restart(int sig) {
    if (stop(sig) == -1) {
        return -1;
    }
    return start();
}

pid_t Process::getpid() const {
    return _pid;
}

std::string Process::get_cmd_path(const std::string& cmd) {
    if (cmd.find('/') != std::string::npos) {
        return cmd;
    }

    char *env_path = std::getenv("PATH");
    if (env_path == nullptr) {
        throw std::runtime_error("Error: please define the PATH env variable");
    }
    std::vector<std::string> path_list = split(env_path, ':');

    for (const auto& path : path_list) {
        std::string cmd_path = path + '/' + cmd;
        if (access(cmd_path.c_str(), X_OK) == 0) {
            return cmd_path;
        }
    }
    throw std::runtime_error("Error: please define the PATH env variable");
}
