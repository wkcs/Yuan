#ifndef YUAN_DRIVER_DRIVER_H
#define YUAN_DRIVER_DRIVER_H

#include "yuan/Driver/Options.h"

namespace yuan {

enum class CompilationResult {
    Success,
    LexerError,
    ParserError,
    SemanticError,
    CodeGenError,
    LinkError,
    IOError,
    InternalError
};

class Driver {
public:
    explicit Driver(const DriverOptions& options);

    CompilationResult run();

    const DriverOptions& getOptions() const { return Options; }

    static int getExitCode(CompilationResult result);

private:
    DriverOptions Options;
};

} // namespace yuan

#endif // YUAN_DRIVER_DRIVER_H
