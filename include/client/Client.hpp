#ifndef CLIENT_HPP
#define CLIENT_HPP
#include <string>

class Client {
public:
    explicit Client(std::string prompt_string);
    void loop();
    void status();
    void start(const std::string& program_name);
    void stop(const std::string& program_name);
    void restart(const std::string& program_name);
    void reload();
    static void print_header();
    static void print_usage();
private:
    std::string _prompt_string;
};

#endif //CLIENT_HPP
