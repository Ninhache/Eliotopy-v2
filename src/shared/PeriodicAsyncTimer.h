#pragma once
#include <chrono>
#include <cstdint>

#pragma once
#include <chrono>
#include <cstdint>
#include <utility>

class PeriodicAsyncTimer {
public:
    template <auto Tag = []{}>
    static bool wait(int64_t milliseconds) {
        static int64_t last_trigger_time = 0;

        auto now = std::chrono::steady_clock::now().time_since_epoch();
        int64_t current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        if (last_trigger_time == 0 || (current_ms - last_trigger_time) >= milliseconds) {
            last_trigger_time = current_ms;
            return true;
        }

        return false;
    }

    template <auto Tag = []{}, typename Callable>
    static void execute(int64_t milliseconds, Callable&& func) {
        static int64_t last_trigger_time = 0;

        auto now = std::chrono::steady_clock::now().time_since_epoch();
        int64_t current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        if (last_trigger_time == 0 || (current_ms - last_trigger_time) >= milliseconds) {
            std::forward<Callable>(func)();
            auto post_now = std::chrono::steady_clock::now().time_since_epoch();
            last_trigger_time = std::chrono::duration_cast<std::chrono::milliseconds>(post_now).count();
        }
    }
};