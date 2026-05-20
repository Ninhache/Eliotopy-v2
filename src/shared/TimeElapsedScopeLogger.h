#pragma once
#include <chrono>
#include <string_view>
#include <source_location>
#include <type_traits>
#include <utility>
#include "AppLogger.h"

class TimeElapsedScopeLogger {
private:
    std::string_view taskName;
    std::source_location loc;
    std::chrono::steady_clock::time_point start;

public:
    TimeElapsedScopeLogger(std::string_view name, const std::source_location& location = std::source_location::current())
            : taskName(name), loc(location), start(std::chrono::steady_clock::now()) {}

    ~TimeElapsedScopeLogger() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        Logger::debug(loc, "Task [{}] executed in {} ms", taskName, duration);
    }

    template<typename Callable>
    static decltype(auto) measure(std::string_view name, Callable&& func, const std::source_location& location = std::source_location::current()) {
        auto start = std::chrono::steady_clock::now();

        if constexpr (std::is_void_v<std::invoke_result_t<Callable>>) {
            std::forward<Callable>(func)();
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Logger::debug(location, "Task [{}] executed in {} ms", name, duration);
        } else {
            decltype(auto) result = std::forward<Callable>(func)();
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Logger::debug(location, "Task [{}] executed in {} ms", name, duration);
            return result;
        }
    }
};