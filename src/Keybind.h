#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include "shared/AppLogger.h"
#include "Config.h"
#include "Utils.h"

struct Keybind {
    int trigger = 0;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    Keybind() = default;
    Keybind(int trigger, bool ctrl = false, bool alt = false, bool shift = false)
        : trigger(trigger), ctrl(ctrl), alt(alt), shift(shift) {}

    bool valid() const { return trigger != 0; }
    bool hasModifier() const { return ctrl || alt || shift; }

    bool operator==(const Keybind& other) const {
        return trigger == other.trigger && ctrl == other.ctrl && alt == other.alt && shift == other.shift;
    }
};

class KeybindManager {
public:
    static constexpr int kMouseWheelUp = 0x110;
    static constexpr int kMouseWheelDown = 0x111;

    using Action = std::function<void()>;
    using CaptureCallback = std::function<void(bool, Keybind)>;

    struct Binding {
        Action onDown;
        Action onUp;
    };

    void init(HWND gameWindow) {
        _gameWindow = gameWindow;
        _instance = this;
        _keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, GetModuleHandleW(nullptr), 0);
        _mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, GetModuleHandleW(nullptr), 0);
        if (!_keyboardHook || !_mouseHook)
            Logger::error("Failed to install keybind hooks");
    }

    void shutdown() {
        if (_keyboardHook) {
            UnhookWindowsHookEx(_keyboardHook);
            _keyboardHook = nullptr;
        }
        if (_mouseHook) {
            UnhookWindowsHookEx(_mouseHook);
            _mouseHook = nullptr;
        }
        if (_instance == this)
            _instance = nullptr;
    }

    void bind(const Keybind& kb, Action onDown, Action onUp = nullptr) {
        _bindings[encode(kb)] = Binding{ std::move(onDown), std::move(onUp) };
    }

    void clear() {
        _bindings.clear();
        _heldUp.clear();
    }

    void beginCapture(CaptureCallback callback) {
        _onCapture = std::move(callback);
        _capturing = true;
    }

    void cancelCapture() {
        finishCapture(false, {});
    }

    bool isCapturing() const { return _capturing; }

    static std::string displayName(const Keybind& kb) {
        std::string name;
        if (kb.ctrl) name += "Ctrl+";
        if (kb.alt) name += "Alt+";
        if (kb.shift) name += "Shift+";
        name += triggerName(kb.trigger);
        return name;
    }

private:
    static inline KeybindManager* _instance = nullptr;

    HHOOK _keyboardHook = nullptr;
    HHOOK _mouseHook = nullptr;
    HWND _gameWindow = nullptr;

    std::unordered_map<uint32_t, Binding> _bindings;
    std::unordered_set<int> _capturedKeys;
    std::unordered_set<int> _capturedMouse;
    std::unordered_map<int, Action> _heldUp;

    bool _capturing = false;
    CaptureCallback _onCapture;

    static uint32_t encode(const Keybind& kb) {
        uint32_t code = static_cast<uint32_t>(kb.trigger & 0xFFFF);
        if (kb.ctrl) code |= (1u << 16);
        if (kb.alt) code |= (1u << 17);
        if (kb.shift) code |= (1u << 18);
        return code;
    }

    static bool isModifier(int vk) {
        switch (vk) {
            case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
            case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
            case VK_MENU: case VK_LMENU: case VK_RMENU:
            case VK_LWIN: case VK_RWIN:
                return true;
            default:
                return false;
        }
    }

    static Keybind withCurrentModifiers(int trigger) {
        Keybind kb;
        kb.trigger = trigger;
        kb.ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        kb.alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        kb.shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        return kb;
    }

    static std::string triggerName(int trigger) {
        switch (trigger) {
            case kMouseWheelUp:  return "WheelUp";
            case kMouseWheelDown:return "WheelDown";
            case VK_LBUTTON:     return "Mouse1";
            case VK_RBUTTON:     return "Mouse2";
            case VK_MBUTTON:     return "Mouse3";
            case VK_XBUTTON1:    return "Mouse4";
            case VK_XBUTTON2:    return "Mouse5";
            case VK_SPACE:       return "Space";
            case VK_TAB:         return "Tab";
            case VK_RETURN:      return "Enter";
            case VK_ESCAPE:      return "Esc";
            case VK_BACK:        return "Backspace";
            case VK_DELETE:      return "Delete";
            default: break;
        }
        if (trigger >= '0' && trigger <= '9') return std::string(1, static_cast<char>(trigger));
        if (trigger >= 'A' && trigger <= 'Z') return std::string(1, static_cast<char>(trigger));
        if (trigger >= VK_NUMPAD0 && trigger <= VK_NUMPAD9) return "Num" + std::to_string(trigger - VK_NUMPAD0);
        if (trigger >= VK_F1 && trigger <= VK_F24) return "F" + std::to_string(trigger - VK_F1 + 1);
        return keyName(trigger);
    }

    static std::string keyName(int vk) {
        UINT scanCode = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        bool extended = (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
                         vk == VK_PRIOR || vk == VK_NEXT || vk == VK_LEFT || vk == VK_RIGHT ||
                         vk == VK_UP || vk == VK_DOWN || vk == VK_NUMLOCK || vk == VK_DIVIDE);
        LONG lParam = static_cast<LONG>(scanCode << 16);
        if (extended)
            lParam |= (1 << 24);

        wchar_t buffer[64] = {};
        if (GetKeyNameTextW(lParam, buffer, 64) > 0)
            return Utils::toUtf8(buffer);
        return "VK" + std::to_string(vk);
    }

    bool inputAllowed() const {
        HWND foreground = GetForegroundWindow();
        if (foreground == _gameWindow)
            return true;
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        return pid == GetCurrentProcessId();
    }

    bool pressDown(const Keybind& kb, int trigger, bool trackHold) {
        auto it = _bindings.find(encode(kb));
        if (it == _bindings.end())
            return false;
        if (it->second.onDown)
            it->second.onDown();
        if (trackHold && it->second.onUp)
            _heldUp[trigger] = it->second.onUp;
        return true;
    }

    void releaseUp(int trigger) {
        auto it = _heldUp.find(trigger);
        if (it == _heldUp.end())
            return;
        Action onUp = it->second;
        _heldUp.erase(it);
        if (onUp)
            onUp();
    }

    void finishCapture(bool success, Keybind kb) {
        if (!_capturing)
            return;
        _capturing = false;
        CaptureCallback callback = std::move(_onCapture);
        _onCapture = nullptr;
        if (callback)
            callback(success, kb);
    }

    static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && _instance) {
            auto* event = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            int vk = static_cast<int>(event->vkCode);
            bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

            if (up && _instance->_capturedKeys.erase(vk) > 0) {
                _instance->releaseUp(vk);
                return 1;
            }

            if (_instance->_capturing) {
                if (down) {
                    if (vk == VK_ESCAPE) {
                        _instance->finishCapture(false, {});
                        return 1;
                    }
                    if (!isModifier(vk)) {
                        _instance->_capturedKeys.insert(vk);
                        _instance->finishCapture(true, withCurrentModifiers(vk));
                        return 1;
                    }
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

            if (down && !isModifier(vk) && _instance->inputAllowed()) {
                if (_instance->_capturedKeys.count(vk) > 0)
                    return 1;
                if (_instance->pressDown(withCurrentModifiers(vk), vk, true)) {
                    _instance->_capturedKeys.insert(vk);
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && _instance) {
            auto* event = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            int trigger = 0;
            bool down = false, up = false, wheel = false;

            switch (wParam) {
                case WM_LBUTTONDOWN: trigger = VK_LBUTTON; down = true; break;
                case WM_LBUTTONUP:   trigger = VK_LBUTTON; up = true; break;
                case WM_RBUTTONDOWN: trigger = VK_RBUTTON; down = true; break;
                case WM_RBUTTONUP:   trigger = VK_RBUTTON; up = true; break;
                case WM_MBUTTONDOWN: trigger = VK_MBUTTON; down = true; break;
                case WM_MBUTTONUP:   trigger = VK_MBUTTON; up = true; break;
                case WM_XBUTTONDOWN: trigger = (HIWORD(event->mouseData) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2; down = true; break;
                case WM_XBUTTONUP:   trigger = (HIWORD(event->mouseData) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2; up = true; break;
                case WM_MOUSEWHEEL:  trigger = (GET_WHEEL_DELTA_WPARAM(event->mouseData) > 0) ? kMouseWheelUp : kMouseWheelDown; wheel = true; break;
                default: break;
            }

            if (up && trigger && _instance->_capturedMouse.erase(trigger) > 0) {
                _instance->releaseUp(trigger);
                return 1;
            }

            if (_instance->_capturing) {
                if (down && trigger) {
                    Keybind kb = withCurrentModifiers(trigger);
                    if (trigger == VK_LBUTTON && !kb.hasModifier()) {
                        _instance->finishCapture(false, {});
                        return CallNextHookEx(nullptr, nCode, wParam, lParam);
                    }
                    _instance->_capturedMouse.insert(trigger);
                    _instance->finishCapture(true, kb);
                    return 1;
                }
                if (wheel && trigger) {
                    _instance->finishCapture(true, withCurrentModifiers(trigger));
                    return 1;
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

            if (_instance->inputAllowed()) {
                if (down && trigger) {
                    Keybind kb = withCurrentModifiers(trigger);
                    if (trigger == VK_LBUTTON && !kb.hasModifier())
                        return CallNextHookEx(nullptr, nCode, wParam, lParam);
                    if (_instance->_capturedMouse.count(trigger) > 0)
                        return 1;
                    if (_instance->pressDown(kb, trigger, true)) {
                        _instance->_capturedMouse.insert(trigger);
                        return 1;
                    }
                } else if (wheel && trigger && _instance->pressDown(withCurrentModifiers(trigger), trigger, false)) {
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
};

class KeybindRegistry {
public:
    struct ActionDef {
        std::string id;
        std::string label;
        Keybind defaultBind;
        KeybindManager::Action onDown;
        KeybindManager::Action onUp;
        Keybind current;
    };

    static KeybindRegistry& get() {
        static KeybindRegistry instance;
        return instance;
    }

    void attach(KeybindManager* manager) { _manager = manager; }

    void registerAction(const std::string& id, const std::string& label, const Keybind& defaultBind, KeybindManager::Action onDown) {
        _actions.push_back(ActionDef{ id, label, defaultBind, std::move(onDown), nullptr, defaultBind });
    }

    void registerHoldAction(const std::string& id, const std::string& label, const Keybind& defaultBind, KeybindManager::Action onDown, KeybindManager::Action onUp) {
        _actions.push_back(ActionDef{ id, label, defaultBind, std::move(onDown), std::move(onUp), defaultBind });
    }

    void loadFromConfig() {
        const auto& stored = ConfigManager::get().state.keybinds;
        for (auto& action : _actions) {
            auto it = stored.find(action.id);
            action.current = (it != stored.end()) ? decode(it->second) : action.defaultBind;
        }
        applyBindings();
    }

    void applyBindings() {
        if (!_manager)
            return;
        _manager->clear();
        for (auto& action : _actions) {
            if (!action.current.valid())
                continue;
            _manager->bind(action.current, action.onDown, action.onUp);
        }
    }

    void rebind(const std::string& id, const Keybind& kb) {
        ActionDef* action = find(id);
        if (!action)
            return;

        if (kb.valid()) {
            for (auto& other : _actions) {
                if (other.id != id && other.current == kb) {
                    other.current = Keybind{};
                    ConfigManager::get().state.keybinds[other.id] = encodeBind(Keybind{});
                }
            }
        }

        action->current = kb;
        ConfigManager::get().state.keybinds[id] = encodeBind(kb);
        ConfigManager::get().save();
        applyBindings();
        _dirty = true;
    }

    void clearBind(const std::string& id) {
        rebind(id, Keybind{});
    }

    void startCapture(const std::string& id) {
        if (!_manager || !find(id))
            return;
        _capturingId = id;
        _dirty = true;
        _manager->beginCapture([this, id](bool success, Keybind kb) {
            _capturingId.clear();
            _dirty = true;
            if (success)
                rebind(id, kb);
        });
    }

    void cancelCapture() {
        if (_manager)
            _manager->cancelCapture();
        _capturingId.clear();
        _dirty = true;
    }

    const std::string& capturingId() const { return _capturingId; }

    bool consumeDirty() {
        bool dirty = _dirty;
        _dirty = false;
        return dirty;
    }

    nlohmann::json toJson() const {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& action : _actions) {
            arr.push_back({
                { "id", action.id },
                { "label", action.label },
                { "bind", action.current.valid() ? KeybindManager::displayName(action.current) : "" }
            });
        }
        return arr;
    }

private:
    KeybindManager* _manager = nullptr;
    std::vector<ActionDef> _actions;
    std::string _capturingId;
    bool _dirty = false;

    ActionDef* find(const std::string& id) {
        for (auto& action : _actions)
            if (action.id == id)
                return &action;
        return nullptr;
    }

    static std::string encodeBind(const Keybind& kb) {
        return std::to_string(kb.trigger) + "," + (kb.ctrl ? "1" : "0") + "," + (kb.alt ? "1" : "0") + "," + (kb.shift ? "1" : "0");
    }

    static Keybind decode(const std::string& str) {
        Keybind kb;
        std::stringstream ss(str);
        std::string token;
        if (std::getline(ss, token, ',') && !token.empty()) kb.trigger = std::stoi(token);
        if (std::getline(ss, token, ',')) kb.ctrl = (token == "1");
        if (std::getline(ss, token, ',')) kb.alt = (token == "1");
        if (std::getline(ss, token, ',')) kb.shift = (token == "1");
        return kb;
    }
};
