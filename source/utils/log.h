#pragma once

#include <pch.h>
#include <utils/utils.h>
#include <utils/nocopyable.h>

inline constexpr auto GetLogSourceLocation(const std::source_location& location = std::source_location::current()) {
    return spdlog::source_loc { location.file_name(), static_cast<int>(location.line()), location.function_name() };
}

inline constexpr std::string_view GetDefaultLogPattern() {
#if defined(_DEBUG) || defined(DEBUG)
    return "%^[%Y-%m-%d %T%e] (%s:%# %!) %l: %v%$";
#else
    return "%^[%Y-%m-%d %T%e] %l: %v%$";
#endif
}

namespace Nanite {
class Logger final: NoCopyable {
public:
    static Logger& GetLogger() {
        static Logger logger;
        return logger;
    }

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;

    void InitLogger(size_t level, std::string_view pattern = GetDefaultLogPattern()) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(static_cast<spdlog::level::level_enum>(level));
        console_sink->set_pattern(std::string(pattern));

        std::vector<spdlog::sink_ptr> sinks { console_sink };
        m_logger = std::make_shared<spdlog::logger>("ConsoleLogger", std::begin(sinks), std::end(sinks));
        m_logger->set_level(static_cast<spdlog::level::level_enum>(level));

        spdlog::set_default_logger(m_logger);
    }

private:
    Logger()  = default;
    ~Logger() = default;

private:
    std::shared_ptr<spdlog::logger> m_logger;
};

// trace
template<typename... Args>
struct LogTrace {
    LogTrace(
        fmt::format_string<Args...> fmt,
        Args&&... args,
        const std::source_location& location = std::source_location::current()
    ) {
        spdlog::log(GetLogSourceLocation(location), spdlog::level::trace, fmt, std::forward<Args>(args)...);
    }
};

template<typename... Args>
LogTrace(fmt::format_string<Args...> fmt, Args&&... args) -> LogTrace<Args...>;

// debug
template<typename... Args>
struct LogDebug {
    LogDebug(
        fmt::format_string<Args...> fmt,
        Args&&... args,
        const std::source_location& location = std::source_location::current()
    ) {
        spdlog::log(GetLogSourceLocation(location), spdlog::level::debug, fmt, std::forward<Args>(args)...);
    }
};

template<typename... Args>
LogDebug(fmt::format_string<Args...> fmt, Args&&... args) -> LogDebug<Args...>;

// info
template<typename... Args>
struct LogInfo {
    LogInfo(
        fmt::format_string<Args...> fmt,
        Args&&... args,
        const std::source_location& location = std::source_location::current()
    ) {
        spdlog::log(GetLogSourceLocation(location), spdlog::level::info, fmt, std::forward<Args>(args)...);
    }
};

template<typename... Args>
LogInfo(fmt::format_string<Args...> fmt, Args&&... args) -> LogInfo<Args...>;

// warn
template<typename... Args>
struct LogWarn {
    LogWarn(
        fmt::format_string<Args...> fmt,
        Args&&... args,
        const std::source_location& location = std::source_location::current()
    ) {
        spdlog::log(GetLogSourceLocation(location), spdlog::level::warn, fmt, std::forward<Args>(args)...);
    }
};

template<typename... Args>
LogWarn(fmt::format_string<Args...> fmt, Args&&... args) -> LogWarn<Args...>;

// error
template<typename... Args>
struct LogError {
    LogError(
        fmt::format_string<Args...> fmt,
        Args&&... args,
        const std::source_location& location = std::source_location::current()
    ) {
        spdlog::log(GetLogSourceLocation(location), spdlog::level::err, fmt, std::forward<Args>(args)...);
    }
};

template<typename... Args>
LogError(fmt::format_string<Args...> fmt, Args&&... args) -> LogError<Args...>;

// critical
template<typename... Args>
struct LogCritical {
    LogCritical(
        fmt::format_string<Args...> fmt,
        Args&&... args,
        const std::source_location& location = std::source_location::current()
    ) {
        spdlog::log(GetLogSourceLocation(location), spdlog::level::critical, fmt, std::forward<Args>(args)...);
    }
};

template<typename... Args>
LogCritical(fmt::format_string<Args...> fmt, Args&&... args) -> LogCritical<Args...>;

} // namespace Nanite

#ifdef NANITE_DEBUG
    #define Check(x) \
        do { \
            if (!(x)) { \
                ::Nanite::LogCritical("Check '{}' failed", #x); \
                if (IsDebuggerAttach()) { \
                    __debugbreak(); \
                } else { \
                    throw std::logic_error("Crash"); \
                } \
            } \
        } while (0)
#else
    #define Check(x)
#endif
