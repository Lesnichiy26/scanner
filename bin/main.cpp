#include <iostream>
#include "../lib/PortChecker.h"
#include <regex>
#include <string>
#include <chrono>


bool isInteger(const std::string& str) {
    try {
        size_t pos;
        std::stoi(str, &pos);
        return pos == str.length();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }
}


int main(int argc, char* argv[]) { // expected this syntax : exe IP start_range end_range ///// so expected argc == 4
    if (argc != 3) {
        std::cerr << "Error : expected 4 arguments\n";
        return EXIT_FAILURE;
    }
    int start_, end_;
    std::string IP = argv[1];
    std::string start = argv[2];
    std::string end = argv[3];
    if (!isInteger(start) || !isInteger(end)) {
        std::cerr << "Error : wrong range\n";
        return EXIT_FAILURE;
    }
    start_ = std::stoi(start);
    end_ = std::stoi(end);

    std::regex ipPattern(R"((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9]))");
    if (!std::regex_match(IP, ipPattern)) {
        std::cerr << "Error : wrong IP\n";
        return EXIT_FAILURE;
    }
    PortChecker checker(IP);
    auto required = checker.checkPortRange(start_, end_);
    return 0;
}
