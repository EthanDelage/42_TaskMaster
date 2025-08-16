#include "client/Client.hpp"

#include <iostream>
#include <common/utils.hpp>

Client::Client(std::string prompt_string): _prompt_string(std::move(prompt_string)) {}

void Client::loop() {
    print_header();

    std::string input;
    while (true) {
        std::cout << _prompt_string;
        if (!std::getline(std::cin, input)) {
            break; // EOF or error
        }

        std::vector<std::string> args = split(input, ' ');
        if (args.empty()) {
            continue;
        }

        const std::string& cmd = args[0];

        if (cmd == "exit" || cmd == "quit") {
            break;
        }
        if (cmd == "status") {
            status();
        } else if (cmd == "start") {
            start(args);
        } else if (cmd == "stop") {
            stop(args);
        } else if (cmd == "restart") {
            restart(args);
        } else if (cmd == "reload") {
            reload();
        } else if (cmd == "help") {
            print_usage();
        } else {
            std::cout << "Unknown command: " << cmd << "\n";
        }
    }
}

void Client::status() {
    std::cout << "Status" << std::endl;
}

void Client::start(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("Invalid number of arguments\n"
                                 "Usage: start <program_name>");
    }
    std::cout << "Starting process: " << args[1] << std::endl;
}

void Client::stop(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("Invalid number of arguments\n"
                                 "Usage: stop <program_name>");
    }
    std::cout << "Stopping process: " << args[1] << std::endl;
}

void Client::restart(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("Invalid number of arguments\n"
                                 "Usage: restart <program_name>");
    }
    std::cout << "Restarting process: " << args[1] << std::endl;
}

void Client::reload() {
    std::cout << "Reloading config" << std::endl;
}

void Client::print_header() {
    std::cout << "=====================================\n";
    std::cout << "         Taskmaster Shell v1.0       \n";
    std::cout << "=====================================\n";
    std::cout << "Type 'help' to list all available commands.\n";
    std::cout << "Press Ctrl-D or Ctrl-C to quit the shell without stopping programs.\n\n";
}

void Client::print_usage() {
    std::cout << "\nAvailable commands:\n"
              << "  status                Show the status of all programs from the config file\n"
              << "  start <name>          Start the specified program\n"
              << "  stop <name>           Stop the specified program\n"
              << "  restart <name>        Restart the specified program\n"
              << "  reload                Reload the configuration file without stopping the shell\n"
              << "  quit                  Stop all programs and exit the shell\n"
              << "  exit                  Same as 'quit'\n"
              << "  help                  Show this help message\n";
}
