#include "syshelpers.h"

#include <array>
#include <iostream>
#include <memory>

bool systemCommand(const std::string &cmd)
{
    if (std::system(nullptr) != 0)
    {
        return std::system(cmd.c_str()) == 0;
    }
    throw std::runtime_error("Command processor not available");
}

std::pair<bool, std::string> systemCommandStdout(const std::string &cmd)
{
    if (std::system(nullptr) != 0)
    {
        std::array<char, 128> buffer{};
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe)
        {
            std::string result;
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            {
                result += buffer.data();
            }
            return std::make_pair(true, result);
        }
        throw std::runtime_error("Failed to open pipe");
    }
    throw std::runtime_error("Command processor not available");
}
