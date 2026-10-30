#include "commandLineApplication.h"

#include "private/commandLineApplicationPrivate.h"

#include <memory>

namespace desktopctl {

CommandLineApplication::CommandLineApplication()
    : _d(std::make_unique<CommandLineApplicationPrivate>())
{
}

CommandLineApplication::~CommandLineApplication() = default;

int CommandLineApplication::run(const std::vector<std::string>& arguments)
{
    return _d->run(arguments);
}

} // namespace desktopctl
