#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>

class Process {
public:
    explicit Process(std::string cmd);
private:
    std::string _cmd; // TODO: change cmd by program_config (when it is implemented)
};



#endif //PROCESS_HPP
