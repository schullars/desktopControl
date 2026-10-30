#pragma once

#include <memory>
#include <string>
#include <vector>

namespace desktopctl {

class CommandLineApplicationPrivate;

class CommandLineApplication final {
public:
    /// <summary>
    /// Creates the command-line application.
    /// </summary>
    CommandLineApplication();
    ~CommandLineApplication();

    CommandLineApplication(const CommandLineApplication&) = delete;
    CommandLineApplication& operator=(const CommandLineApplication&) = delete;

    /// <summary>
    /// Parses and executes one command.
    /// </summary>
    /// <param name="arguments">UTF-8 arguments with the executable name at index zero.</param>
    /// <returns>0 on success, 2 for invalid arguments, or 3 for an operation failure.</returns>
    int run(const std::vector<std::string>& arguments);

private:
    std::unique_ptr<CommandLineApplicationPrivate> _d;
};

} // namespace desktopctl
