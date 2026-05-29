#pragma once
#include <windows.h>
#include <string>
#include <random>

namespace Utils {

inline std::string randomString(size_t length) {
    static constexpr char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string str(length, '\0');
    for (auto& ch : str)
        ch = charset[dist(rng)];
    return str;
}

inline std::string toUtf8(const std::wstring& text) {
    if (text.empty())
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), size, nullptr, nullptr);
    return out;
}

inline std::wstring toWide(const std::string& text) {
    if (text.empty())
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring out(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), size);
    return out;
}

}
