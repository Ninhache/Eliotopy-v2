#pragma once
#include <iostream>
#include <string_view>
#include <source_location>
#include <format>
#include <chrono>

enum class LogLevel { debug, info, warn, error };

class Logger {
private:
    struct ConsoleInitializer {
        ConsoleInitializer() {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD dwMode = 0;
                if (GetConsoleMode(hOut, &dwMode)) {
                    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    SetConsoleMode(hOut, dwMode);
                }
            }
        }
    };

    static inline ConsoleInitializer initializer;
    static inline LogLevel minLevel = LogLevel::info;
    static inline FILE* consoleFpOut = nullptr;
    static inline FILE* consoleFpErr = nullptr;

    static constexpr std::string_view resetColor = "\033[0m";
    static constexpr std::string_view debugColor = "\033[90m";
    static constexpr std::string_view infoColor = "\033[32m";
    static constexpr std::string_view warnColor = "\033[33m";
    static constexpr std::string_view errorColor = "\033[31m";

    static std::string_view getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::debug: return "DEBUG";
            case LogLevel::info:  return "INFO ";
            case LogLevel::warn:  return "WARN ";
            case LogLevel::error: return "ERROR";
            default:              return "NONE ";
        }
    }

    static std::string_view getLevelColor(LogLevel level) {
        switch (level) {
            case LogLevel::debug: return debugColor;
            case LogLevel::info:  return infoColor;
            case LogLevel::warn:  return warnColor;
            case LogLevel::error: return errorColor;
            default:              return resetColor;
        }
    }

    static std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        struct tm tmStruct;
        localtime_s(&tmStruct, &time);
        return std::format("{:02}:{:02}:{:02}", tmStruct.tm_hour, tmStruct.tm_min, tmStruct.tm_sec);
    }

    static std::string extractFileName(std::string_view path) {
        size_t lastSlash = path.find_last_of("/\\");
        return (lastSlash != std::string_view::npos) ? std::string(path.substr(lastSlash + 1)) : std::string(path);
    }

private:
    static void hideFromTaskbar(HWND hwnd) {
        if (!hwnd) return;
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle = (exStyle | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW;
        ShowWindow(hwnd, SW_HIDE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
        ShowWindow(hwnd, SW_SHOW);
    }

public:
    static void allocateConsole(HWND parent = nullptr) {
        HWND hwnd = GetConsoleWindow();
        if (hwnd != nullptr) {
            hideFromTaskbar(hwnd);
            if (parent != nullptr) {
                RECT parentRect, consoleRect;
                if (GetWindowRect(parent, &parentRect) && GetWindowRect(hwnd, &consoleRect)) {
                    int w = consoleRect.right - consoleRect.left;
                    int h = consoleRect.bottom - consoleRect.top;
                    int x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
                    int y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;
                    SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE);
                }
            }
            return;
        }
        
        AllocConsole();
        freopen_s(&consoleFpOut, "CONOUT$", "w", stdout);
        freopen_s(&consoleFpErr, "CONOUT$", "w", stderr);
        
        HANDLE hOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
            SetStdHandle(STD_OUTPUT_HANDLE, hOut);
            SetStdHandle(STD_ERROR_HANDLE, hOut);
        }
        
        std::cout.clear();
        std::cerr.clear();
        std::clog.clear();

        hwnd = GetConsoleWindow();
        if (hwnd) {
            hideFromTaskbar(hwnd);
            int x = 0, y = 0;
            UINT flags = SWP_NOMOVE | SWP_NOSIZE;
            if (parent != nullptr) {
                RECT parentRect, consoleRect;
                if (GetWindowRect(parent, &parentRect) && GetWindowRect(hwnd, &consoleRect)) {
                    int w = consoleRect.right - consoleRect.left;
                    int h = consoleRect.bottom - consoleRect.top;
                    x = parentRect.left + (parentRect.right - parentRect.left - w) / 2;
                    y = parentRect.top + (parentRect.bottom - parentRect.top - h) / 2;
                    flags = SWP_NOSIZE;
                }
            }
            SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, flags);

            HMENU hMenu = GetSystemMenu(hwnd, FALSE);
            if (hMenu) {
                EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
            }

            const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
            std::string title;
            srand((unsigned int)time(NULL));
            for (int i = 0; i < 16; ++i) {
                title += charset[rand() % (sizeof(charset) - 1)];
            }
            SetConsoleTitleA(title.c_str());
        }
    }

    static void freeConsole() {
        HWND hwnd = GetConsoleWindow();
        if (hwnd != nullptr) {
            ShowWindow(hwnd, SW_HIDE);
        }
    }

    static void setLevel(LogLevel level) { minLevel = level; }

    template <typename... Args>
    static void log(LogLevel level, const std::source_location& loc, std::string_view fmt, Args&&... args) {
        if (level < minLevel) return;

        std::string formatted = std::vformat(fmt, std::make_format_args(args...));
        std::string time = getCurrentTime();
        std::string_view lvlStr = getLevelString(level);
        std::string_view lvlCol = getLevelColor(level);

        if (level >= LogLevel::warn) {
            std::cout << std::format("{}[{}] [{}] [{}:{}] {}{}\n",
                                     lvlCol, time, lvlStr, extractFileName(loc.file_name()), loc.line(), formatted, resetColor);
        } else {
            std::cout << std::format("{}[{}] [{}] {}{}\n",
                                     lvlCol, time, lvlStr, formatted, resetColor);
        }
    }

    template <typename... Args>
    static void debug(const std::source_location& loc, std::string_view fmt, Args&&... args) {
        log(LogLevel::debug, loc, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void info(const std::source_location& loc, std::string_view fmt, Args&&... args) {
        log(LogLevel::info, loc, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void warn(const std::source_location& loc, std::string_view fmt, Args&&... args) {
        log(LogLevel::warn, loc, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void error(const std::source_location& loc, std::string_view fmt, Args&&... args) {
        log(LogLevel::error, loc, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void debug(std::string_view fmt, Args&&... args) { debug(std::source_location::current(), fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    static void info(std::string_view fmt, Args&&... args) { info(std::source_location::current(), fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    static void warn(std::string_view fmt, Args&&... args) { warn(std::source_location::current(), fmt, std::forward<Args>(args)...); }

    template <typename... Args>
    static void error(std::string_view fmt, Args&&... args) { error(std::source_location::current(), fmt, std::forward<Args>(args)...); }
};