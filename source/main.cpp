#include <utils/log.h>

void TestCheckMacro(bool shouldFail) {
    if (shouldFail) {
        std::cout << "Testing Check macro failure (this should cause an error)..." << std::endl;
        Check(false);
    } else {
        std::cout << "Testing Check macro success..." << std::endl;
        Check(true);
    }
}

int main() {
    try {
        std::cout << "Initializing logger..." << std::endl;
        Nanite::Logger::GetLogger().InitLogger(0);

        std::cout << "\nTesting basic log messages:\n" << std::endl;
        Nanite::LogTrace("This is a trace message");
        Nanite::LogDebug("This is a debug message");
        Nanite::LogInfo("This is an info message");
        Nanite::LogWarn("This is a warning message");
        Nanite::LogError("This is an error message");
        Nanite::LogCritical("This is a critical message");

        std::cout << "\nTesting log messages with formatting:\n" << std::endl;
        Nanite::LogInfo("Integer value: {}", 42);
        Nanite::LogInfo("Float value: {:.2f}", 3.14159);
        Nanite::LogInfo("String value: {}", "hello world");
        Nanite::LogInfo("Multiple values: {}, {}, {}", 100, "mixed types", 99.9);

        Nanite::LogDebug("Result of calculation: {}", std::sqrt(16) + 10);

        std::cout << "\nTesting Check macro:\n" << std::endl;

        TestCheckMacro(false);

        TestCheckMacro(true);

        std::cout << "\nAll tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
