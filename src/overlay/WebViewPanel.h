#pragma once
#include <windows.h>
#include <wrl.h>
#include <string>
#include <random>
#include <functional>
#include <unordered_map>
#include <WebView2.h>
#include "datastore/GameState.h"
#include "Config.h"

using namespace Microsoft::WRL;

#include "WebAssets.h"

#define WM_TOGGLE_COLLAPSE (WM_USER + 1)

class WebViewPanel {
private:
    HWND _panelHwnd = nullptr;
    ComPtr<ICoreWebView2Controller> _controller;
    ComPtr<ICoreWebView2> _webview;
    bool _initialized = false;
    bool _webviewReady = false;
    bool _collapsed = false;
    std::string _className;
    std::string _lastJson;
    HWND _ownerHwnd = nullptr;

    using MessageHandler = std::function<void(const std::wstring&)>;
    std::unordered_map<std::wstring, MessageHandler> _messageHandlers;

    void registerHandlers() {
        _messageHandlers[L"drag"] = [this](const std::wstring&) {
            ReleaseCapture();
            SendMessage(_panelHwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            
            RECT panelRect, gameRect;
            if (GetWindowRect(_panelHwnd, &panelRect) && GetClientRect(_ownerHwnd, &gameRect)) {
                POINT tl = {gameRect.left, gameRect.top};
                ClientToScreen(_ownerHwnd, &tl);
                ConfigManager::get().state.menuX = panelRect.left - tl.x;
                ConfigManager::get().state.menuY = panelRect.top - tl.y;
                ConfigManager::get().save();
            }
        };

        _messageHandlers[L"toggle"] = [this](const std::wstring&) {
            PostMessage(_panelHwnd, WM_TOGGLE_COLLAPSE, 0, 0);
        };

        _messageHandlers[L"exit"] = [](const std::wstring&) {
            PostQuitMessage(0);
        };

        _messageHandlers[L"ready"] = [this](const std::wstring&) {
            _webviewReady = true;
            _lastJson.clear();
        };

        _messageHandlers[L"console"] = [this](const std::wstring& payload) {
            if (payload == L"true") {
                Logger::allocateConsole(_ownerHwnd);
            } else {
                Logger::freeConsole();
            }
        };

        _messageHandlers[L"track"] = [](const std::wstring& payload) {
            try {
                ConfigManager::get().state.trackedEntityId = std::stoll(payload);
                ConfigManager::get().save();
            } catch (...) {}
        };

        _messageHandlers[L"loglevel"] = [](const std::wstring& payload) {
            if (payload == L"debug") Logger::setLevel(LogLevel::debug);
            else if (payload == L"info") Logger::setLevel(LogLevel::info);
            else if (payload == L"warn") Logger::setLevel(LogLevel::warn);
            else if (payload == L"error") Logger::setLevel(LogLevel::error);
        };

        _messageHandlers[L"poll"] = [](const std::wstring& payload) {
            try {
                g_memoryPollRate = std::stoi(payload);
            } catch (...) {}
        };

        _messageHandlers[L"config"] = [](const std::wstring& payload) {
            size_t startPos = payload.find(L"{");
            if (startPos == std::wstring::npos) return;
            
            std::wstring jsonStr = payload.substr(startPos);
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &jsonStr[0], (int)jsonStr.size(), NULL, 0, NULL, NULL);
            std::string jsonUtf8(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, &jsonStr[0], (int)jsonStr.size(), &jsonUtf8[0], size_needed, NULL, NULL);
            
            try {
                nlohmann::json parsed = nlohmann::json::parse(jsonUtf8);
                if (parsed.contains("gridOverlay")) ConfigManager::get().state.gridOverlay = parsed["gridOverlay"].get<bool>();
                if (parsed.contains("gridColor")) ConfigManager::get().state.gridColor = parsed["gridColor"].get<std::string>();
                if (parsed.contains("gridMode")) ConfigManager::get().state.gridMode = parsed["gridMode"].get<std::string>();
                if (parsed.contains("gridShowLos")) ConfigManager::get().state.gridShowLos = parsed["gridShowLos"].get<bool>();
                if (parsed.contains("gridShowMove")) ConfigManager::get().state.gridShowMove = parsed["gridShowMove"].get<bool>();
                if (parsed.contains("gridFillOpacity")) ConfigManager::get().state.gridFillOpacity = parsed["gridFillOpacity"].get<int>();
                if (parsed.contains("gridFillColor")) ConfigManager::get().state.gridFillColor = parsed["gridFillColor"].get<std::string>();
                if (parsed.contains("backgroundOverlay")) ConfigManager::get().state.backgroundOverlay = parsed["backgroundOverlay"].get<int>();
                
                ConfigManager::get().save();
            } catch (...) {}
        };
    }

    void handleMessage(const std::wstring& message) {
        std::wstring command;
        std::wstring payload;
        
        size_t colonPos = message.find(L':');
        if (colonPos != std::wstring::npos) {
            command = message.substr(0, colonPos);
            payload = message.substr(colonPos + 1);
        } else {
            command = message;
        }

        auto it = _messageHandlers.find(command);
        if (it != _messageHandlers.end()) {
            it->second(payload);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<WebViewPanel*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
        if (uMsg == WM_TOGGLE_COLLAPSE) {
            if (!self)
                return 0;
                
            self->_collapsed = !self->_collapsed;
            int width = self->_collapsed ? 140 : 320;
            int height = self->_collapsed ? 36 : 600;
            
            SetWindowPos(
                hwnd, 
                HWND_TOP, 
                0, 0, 
                width, height, 
                SWP_NOMOVE | SWP_NOACTIVATE | SWP_FRAMECHANGED
            );
            
            self->syncWebViewBounds();
            
            RedrawWindow(
                hwnd, 
                nullptr, 
                nullptr, 
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN
            );
            
            self->_lastJson.clear();
            return 0;
        }

        if (uMsg == WM_SIZE) {
            if (self) {
                self->syncWebViewBounds();
            }
            return 0;
        }

        if (uMsg == WM_WINDOWPOSCHANGING) {
            if (self && self->_ownerHwnd) {
                WINDOWPOS* pos = (WINDOWPOS*)lParam;
                if (!(pos->flags & SWP_NOMOVE)) {
                    RECT parentRect;
                    if (GetClientRect(self->_ownerHwnd, &parentRect)) {
                        POINT tl = { parentRect.left, parentRect.top };
                        POINT br = { parentRect.right, parentRect.bottom };
                        ClientToScreen(self->_ownerHwnd, &tl);
                        ClientToScreen(self->_ownerHwnd, &br);

                        if (pos->x < tl.x) pos->x = tl.x;
                        if (pos->y < tl.y) pos->y = tl.y;
                        if (pos->x + pos->cx > br.x) pos->x = br.x - pos->cx;
                        if (pos->y + pos->cy > br.y) pos->y = br.y - pos->cy;
                    }
                }
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        if (uMsg == WM_MOVE) {
            if (self) {
                self->syncWebViewBounds();
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        
        if (uMsg == WM_SETCURSOR) {
            SetCursor(NULL);
            return TRUE;
        }

        if (uMsg == WM_PAINT) {
            if (!self || !self->_webviewReady) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc;
                GetClientRect(hwnd, &rc);
                HBRUSH bg = CreateSolidBrush(RGB(15, 15, 20));
                FillRect(hdc, &rc, bg);
                DeleteObject(bg);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(165, 180, 252));
                HFONT font = CreateFontA(14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
                HGDIOBJ oldFont = SelectObject(hdc, font);
                DrawTextA(hdc, "Loading Eliotopy...", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oldFont);
                DeleteObject(font);
                EndPaint(hwnd, &ps);
                return 0;
            }

            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        if (uMsg == WM_DESTROY)
            return 0;
            
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    std::string GenerateRandomString(size_t length) {
        const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::default_random_engine rng(std::random_device{}());
        std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
        std::string str;
        for (size_t i = 0; i < length; ++i)
            str += charset[dist(rng)];
        return str;
    }

    std::wstring buildHtml() {
        std::string rawHtml(reinterpret_cast<const char*>(webview::web_html_data));
        return std::wstring(rawHtml.begin(), rawHtml.end());
    }

    void createHostWindow(HWND gameWindow) {
        _ownerHwnd = gameWindow;
        _className = GenerateRandomString(12);

        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = _className.c_str();
        wc.hbrBackground = CreateSolidBrush(RGB(30, 31, 34));
        RegisterClassExA(&wc);

        int panelW = 320;
        int panelH = 600;
        RECT gameRect;
        GetWindowRect(gameWindow, &gameRect);
        int gameCenterX = (gameRect.left + gameRect.right) / 2;
        int gameCenterY = (gameRect.top + gameRect.bottom) / 2;

        int startX = gameCenterX - panelW / 2;
        int startY = gameCenterY - panelH / 2;

        if (ConfigManager::get().state.menuX != -1 && ConfigManager::get().state.menuY != -1) {
            POINT tl = {gameRect.left, gameRect.top};
            ClientToScreen(gameWindow, &tl);
            startX = tl.x + ConfigManager::get().state.menuX;
            startY = tl.y + ConfigManager::get().state.menuY;
        }

        _panelHwnd = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            _className.c_str(),
            "",
            WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
            startX, startY, panelW, panelH,
            _ownerHwnd, nullptr, wc.hInstance, nullptr
        );

        SetWindowLongPtrA(_panelHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        ShowWindow(_panelHwnd, SW_SHOW);
        UpdateWindow(_panelHwnd);
    }

    void configureWebViewSettings() {
        ICoreWebView2Settings* settings = nullptr;
        _webview->get_Settings(&settings);
        
        if (settings) {
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_AreDevToolsEnabled(FALSE);
        }

        ComPtr<ICoreWebView2Controller2> controller2;
        _controller->QueryInterface(IID_PPV_ARGS(&controller2));
        
        if (controller2) {
            COREWEBVIEW2_COLOR transparent = {0, 0, 0, 0};
            controller2->put_DefaultBackgroundColor(transparent);
        }
    }

    void setupMessageHandler() {
        registerHandlers();
        
        EventRegistrationToken token;
        _webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    LPWSTR messageRaw = nullptr;
                    if (SUCCEEDED(args->TryGetWebMessageAsString(&messageRaw))) {
                        std::wstring message = messageRaw;
                        CoTaskMemFree(messageRaw);
                        handleMessage(message);
                    }
                    return S_OK;
                }
            ).Get(), &token
        );
    }

    HRESULT onControllerCreated(HRESULT result, ICoreWebView2Controller* controller) {
        if (FAILED(result) || !controller)
            return result;

        _controller = controller;
        _controller->get_CoreWebView2(&_webview);

        syncWebViewBounds();

        configureWebViewSettings();
        setupMessageHandler();

        std::wstring html = buildHtml();
        _webview->NavigateToString(html.c_str());

        return S_OK;
    }

    HRESULT onEnvironmentCreated(HRESULT result, ICoreWebView2Environment* env) {
        if (FAILED(result))
            return result;

        env->CreateCoreWebView2Controller(_panelHwnd,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [this](HRESULT r, ICoreWebView2Controller* c) { return onControllerCreated(r, c); }
            ).Get()
        );
        
        return S_OK;
    }

public:
    void init(HWND gameWindow) {
        createHostWindow(gameWindow);

        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring userDataFolder = std::wstring(tempPath) + L"EliotopyWebViewCache";

        CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder.c_str(), nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT r, ICoreWebView2Environment* e) { return onEnvironmentCreated(r, e); }
            ).Get()
        );

        _initialized = true;
    }

    void update(const GameState& state) {
        if (!_initialized || !_webviewReady || !_webview)
            return;
            
        constrainToBounds();

        static auto lastUpdate = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 200)
            return;
            
        lastUpdate = now;

        nlohmann::json j;
        j["state"] = state;
        j["config"] = ConfigManager::get().state;
        
        std::string json = j.dump();

        if (json == _lastJson)
            return;
            
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, json.data(), (int)json.size(), NULL, 0);
        std::wstring wjson(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, json.data(), (int)json.size(), wjson.data(), size_needed);
        
        HRESULT hr = _webview->PostWebMessageAsString(wjson.c_str());
        
        if (SUCCEEDED(hr)) {
            _lastJson = json;
        } else {
            _lastJson.clear();
        }
    }

private:
    void syncWebViewBounds() {
        if (!_controller || !_panelHwnd)
            return;

        RECT bounds;
        GetClientRect(_panelHwnd, &bounds);
        _controller->put_Bounds(bounds);
        _controller->NotifyParentWindowPositionChanged();
    }

    void constrainToBounds() {
        if (!_ownerHwnd || !_panelHwnd) return;
        RECT parentRect;
        if (GetClientRect(_ownerHwnd, &parentRect)) {
            POINT tl = { parentRect.left, parentRect.top };
            POINT br = { parentRect.right, parentRect.bottom };
            ClientToScreen(_ownerHwnd, &tl);
            ClientToScreen(_ownerHwnd, &br);

            RECT myRect;
            GetWindowRect(_panelHwnd, &myRect);
            int cx = myRect.right - myRect.left;
            int cy = myRect.bottom - myRect.top;
            int x = myRect.left;
            int y = myRect.top;
            
            bool changed = false;
            if (x < tl.x) { x = tl.x; changed = true; }
            if (y < tl.y) { y = tl.y; changed = true; }
            if (x + cx > br.x) { x = br.x - cx; changed = true; }
            if (y + cy > br.y) { y = br.y - cy; changed = true; }
            
            if (changed) {
                SetWindowPos(_panelHwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
};
