// System helper utilities. Should maybe be in a seperate repo...
#pragma once

#include <string>
#include <utility>

/// @brief Run system command. Will return true if command was sucessfully run.
bool systemCommand(const std::string &cmd);

/// @brief Run system command an return result from stdout. Will return <true, ...> if command was sucessfully run.
std::pair<bool, std::string> systemCommandStdout(const std::string &cmd);
