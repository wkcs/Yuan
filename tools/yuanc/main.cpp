#include "yuan/Driver/Driver.h"
#include "yuan/Driver/Options.h"
#include <exception>
#include <iostream>

using namespace yuan;

int main(int argc, char* argv[]) {
    DriverOptions options;
    std::string errorMsg;
    if (!parseDriverOptions(argc, argv, options, errorMsg)) {
        std::cerr << errorMsg << "\n";
        return 1;
    }

    if (options.ShowHelp) {
        printDriverHelp(argv[0], std::cout);
        return 0;
    }
    if (options.ShowVersion) {
        printDriverVersion(std::cout);
        return 0;
    }

    try {
        Driver driver(options);
        CompilationResult result = driver.run();
        return Driver::getExitCode(result);
    } catch (const std::exception& e) {
        std::cerr << "内部错误: " << e.what() << "\n";
        return Driver::getExitCode(CompilationResult::InternalError);
    } catch (...) {
        std::cerr << "未知内部错误\n";
        return Driver::getExitCode(CompilationResult::InternalError);
    }
}
