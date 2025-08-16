#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>

class Client {
public:
    explicit Client(std::string prompt_string);
    void loop();
    void status();
    void start(const std::vector<std::string>& args);
    void stop(const std::vector<std::string>& args);
    void restart(const std::vector<std::string>& args);
    void reload();
    static void print_header();
    static void print_usage();
private:
    std::string _prompt_string;
};

#endif //CLIENT_HPP
