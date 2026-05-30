#pragma once
#include <windows.h>
#include <wrl.h>
#include <string>
#include <functional>
#include <unordered_map>
#include <WebView2.h>
#include "datastore/GameState.h"
#include "Config.h"
#include "Utils.h"
#include "Keybind.h"
#include "Version.h"

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

    enum class DockEdge { Top, Bottom, Left, Right };

    static constexpr int kPanelW = 320;
    static constexpr int kPanelH = 600;
    static constexpr int kPillLong = 86;
    static constexpr int kPillShort = 20;
    static constexpr int kPanelRadius = 16;
    static constexpr int kPillMarginTop = 10;

    DockEdge _dockEdge = DockEdge::Top;

    using MessageHandler = std::function<void(const std::wstring&)>;
    std::unordered_map<std::wstring, MessageHandler> _messageHandlers;

    void registerHandlers() {
        _messageHandlers[L"drag"] = [this](const std::wstring&) {
            RECT before;
            GetWindowRect(_panelHwnd, &before);

            ReleaseCapture();
            SendMessage(_panelHwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);

            RECT after;
            GetWindowRect(_panelHwnd, &after);
            bool moved = (before.left != after.left || before.top != after.top);

            if (_collapsed) {
                if (moved) {
                    reorientCollapsed();
                    savePosition();
                    postUiState();
                } else {
                    PostMessage(_panelHwnd, WM_TOGGLE_COLLAPSE, 0, 0);
                }
            } else {
                savePosition();
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

        _messageHandlers[L"kbcapture"] = [](const std::wstring& payload) {
            KeybindRegistry::get().startCapture(Utils::toUtf8(payload));
        };

        _messageHandlers[L"kbclear"] = [](const std::wstring& payload) {
            KeybindRegistry::get().clearBind(Utils::toUtf8(payload));
        };

        _messageHandlers[L"kbcancel"] = [](const std::wstring&) {
            KeybindRegistry::get().cancelCapture();
        };

        _messageHandlers[L"loglevel"] = [](const std::wstring& payload) {
            if (payload == L"debug") Logger::setLevel(LogLevel::debug);
            else if (payload == L"info") Logger::setLevel(LogLevel::info);
            else if (payload == L"warn") Logger::setLevel(LogLevel::warn);
            else if (payload == L"error") Logger::setLevel(LogLevel::error);
        };

        _messageHandlers[L"poll"] = [](const std::wstring& payload) {
            try {
                int rate = std::stoi(payload);
                g_memoryPollRate = rate;
                auto& config = ConfigManager::get();
                config.state.memoryPollRate = rate;
                config.save();
            } catch (...) {}
        };

        _messageHandlers[L"config"] = [](const std::wstring& payload) {
            size_t startPos = payload.find(L"{");
            if (startPos == std::wstring::npos) return;

            try {
                nlohmann::json patch = nlohmann::json::parse(Utils::toUtf8(payload.substr(startPos)));
                auto& config = ConfigManager::get();
                nlohmann::json merged = config.state;
                merged.merge_patch(patch);
                merged.get_to(config.state);
                config.save();
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
            self->applyLayout();
            self->syncWebViewBounds();
            self->postUiState();

            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

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
                if (!(pos->flags & SWP_NOMOVE))
                    self->snapToEdge(pos);
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

    std::wstring buildHtml() {
        std::string html(reinterpret_cast<const char*>(webview::web_html_data));
        const std::string token = "{{VERSION}}";
        size_t pos = html.find(token);
        if (pos != std::string::npos)
            html.replace(pos, token.size(), std::string("v") + ELIOTOPY_VERSION);
        return Utils::toWide(html);
    }

    void createHostWindow(HWND gameWindow) {
        _ownerHwnd = gameWindow;
        _className = Utils::randomString(12);

        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = _className.c_str();
        wc.hbrBackground = CreateSolidBrush(RGB(30, 31, 34));
        RegisterClassExA(&wc);

        int panelW = kPanelW;
        int panelH = kPanelH;
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
        applyLayout();
        ShowWindow(_panelHwnd, SW_SHOW);
        UpdateWindow(_panelHwnd);
    }

    static const char* dockName(DockEdge edge) {
        switch (edge) {
            case DockEdge::Bottom: return "bottom";
            case DockEdge::Left:   return "left";
            case DockEdge::Right:  return "right";
            default:               return "top";
        }
    }

    static int clampInt(int v, int lo, int hi) {
        if (hi < lo) return lo;
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    DockEdge nearestEdge(int x, int y, int w, int h, const POINT& tl, const POINT& br) const {
        int distLeft = x - tl.x;
        int distRight = (br.x - w) - x;
        int distTop = y - tl.y;
        int distBottom = (br.y - h) - y;

        DockEdge edge = DockEdge::Left;
        int minDist = distLeft;
        if (distRight < minDist) { minDist = distRight; edge = DockEdge::Right; }
        if (distTop < minDist) { minDist = distTop; edge = DockEdge::Top; }
        if (distBottom < minDist) { minDist = distBottom; edge = DockEdge::Bottom; }
        return edge;
    }

    void setCollapsedRegion(int w, int h, DockEdge edge) {
        int diameter = kPillShort;
        HRGN rounded = CreateRoundRectRgn(0, 0, w + 1, h + 1, diameter, diameter);

        HRGN square = nullptr;
        switch (edge) {
            case DockEdge::Top:    square = CreateRectRgn(0, 0, w + 1, h / 2 + 1); break;
            case DockEdge::Bottom: square = CreateRectRgn(0, h / 2, w + 1, h + 1); break;
            case DockEdge::Left:   square = CreateRectRgn(0, 0, w / 2 + 1, h + 1); break;
            case DockEdge::Right:  square = CreateRectRgn(w / 2, 0, w + 1, h + 1); break;
        }

        CombineRgn(rounded, rounded, square, RGN_OR);
        DeleteObject(square);
        SetWindowRgn(_panelHwnd, rounded, TRUE);
    }

    void applyLayout() {
        if (!_panelHwnd || !_ownerHwnd)
            return;

        RECT clientRect;
        GetClientRect(_ownerHwnd, &clientRect);
        POINT tl = { clientRect.left, clientRect.top };
        POINT br = { clientRect.right, clientRect.bottom };
        ClientToScreen(_ownerHwnd, &tl);
        ClientToScreen(_ownerHwnd, &br);
        int gameW = clientRect.right - clientRect.left;

        const auto& state = ConfigManager::get().state;
        int savedX = _collapsed ? state.collapsedX : state.menuX;
        int savedY = _collapsed ? state.collapsedY : state.menuY;

        int x = (savedX != -1) ? tl.x + savedX : tl.x + (gameW - kPanelW) / 2;
        int y = (savedY != -1) ? tl.y + savedY : tl.y + kPillMarginTop;

        if (!_collapsed) {
            _dockEdge = nearestEdge(x, y, kPanelW, kPanelH, tl, br);
            SetWindowRgn(_panelHwnd, CreateRoundRectRgn(0, 0, kPanelW + 1, kPanelH + 1, kPanelRadius, kPanelRadius), TRUE);
            SetWindowPos(_panelHwnd, HWND_TOP, x, y, kPanelW, kPanelH, SWP_NOACTIVATE | SWP_FRAMECHANGED);
            return;
        }

        _dockEdge = nearestEdge(x, y, kPillShort, kPillShort, tl, br);
        bool horizontal = (_dockEdge == DockEdge::Top || _dockEdge == DockEdge::Bottom);
        int w = horizontal ? kPillLong : kPillShort;
        int h = horizontal ? kPillShort : kPillLong;

        setCollapsedRegion(w, h, _dockEdge);
        SetWindowPos(_panelHwnd, HWND_TOP, x, y, w, h, SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    void reorientCollapsed() {
        if (!_collapsed)
            return;

        bool horizontal = (_dockEdge == DockEdge::Top || _dockEdge == DockEdge::Bottom);
        int w = horizontal ? kPillLong : kPillShort;
        int h = horizontal ? kPillShort : kPillLong;

        RECT wr;
        GetWindowRect(_panelHwnd, &wr);
        setCollapsedRegion(w, h, _dockEdge);
        SetWindowPos(_panelHwnd, HWND_TOP, wr.left, wr.top, w, h, SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    void snapToEdge(WINDOWPOS* pos) {
        RECT clientRect;
        if (!GetClientRect(_ownerHwnd, &clientRect))
            return;

        POINT tl = { clientRect.left, clientRect.top };
        POINT br = { clientRect.right, clientRect.bottom };
        ClientToScreen(_ownerHwnd, &tl);
        ClientToScreen(_ownerHwnd, &br);

        int cx = pos->cx;
        int cy = pos->cy;
        if (pos->flags & SWP_NOSIZE) {
            RECT wr;
            GetWindowRect(_panelHwnd, &wr);
            cx = wr.right - wr.left;
            cy = wr.bottom - wr.top;
        }

        int x = pos->x;
        int y = pos->y;

        if (!_collapsed) {
            pos->x = clampInt(x, tl.x, br.x - cx);
            pos->y = clampInt(y, tl.y, br.y - cy);
            return;
        }

        DockEdge edge = nearestEdge(x, y, cx, cy, tl, br);
        bool horizontal = (edge == DockEdge::Top || edge == DockEdge::Bottom);
        int w = horizontal ? kPillLong : kPillShort;
        int h = horizontal ? kPillShort : kPillLong;

        if (w != cx || h != cy) {
            pos->cx = w;
            pos->cy = h;
            pos->flags &= ~SWP_NOSIZE;
            cx = w;
            cy = h;
        }

        if (edge != _dockEdge) {
            _dockEdge = edge;
            setCollapsedRegion(w, h, edge);
            postUiState();
        }

        switch (edge) {
            case DockEdge::Left:   x = tl.x;       y = clampInt(y, tl.y, br.y - cy); break;
            case DockEdge::Right:  x = br.x - cx;  y = clampInt(y, tl.y, br.y - cy); break;
            case DockEdge::Top:    y = tl.y;       x = clampInt(x, tl.x, br.x - cx); break;
            case DockEdge::Bottom: y = br.y - cy;  x = clampInt(x, tl.x, br.x - cx); break;
        }

        pos->x = x;
        pos->y = y;
    }

    void savePosition() {
        if (!_panelHwnd || !_ownerHwnd)
            return;

        RECT panelRect, gameRect;
        if (!GetWindowRect(_panelHwnd, &panelRect) || !GetClientRect(_ownerHwnd, &gameRect))
            return;

        POINT tl = { gameRect.left, gameRect.top };
        ClientToScreen(_ownerHwnd, &tl);
        int relX = panelRect.left - tl.x;
        int relY = panelRect.top - tl.y;

        auto& state = ConfigManager::get().state;
        if (_collapsed) {
            state.collapsedX = relX;
            state.collapsedY = relY;
        } else {
            state.menuX = relX;
            state.menuY = relY;
        }
        ConfigManager::get().save();
    }

    void postUiState() {
        if (!_webview)
            return;

        std::string json = std::string("{\"ui\":{\"collapsed\":") + (_collapsed ? "true" : "false") +
                           ",\"dock\":\"" + dockName(_dockEdge) + "\"}}";

        std::wstring wjson = Utils::toWide(json);
        _webview->PostWebMessageAsString(wjson.c_str());
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

    std::wstring cacheFolderName() {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring name = path;
        size_t slash = name.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            name = name.substr(slash + 1);

        unsigned long long hash = 1469598103934665603ull;
        for (wchar_t c : name) {
            hash ^= static_cast<unsigned short>(c);
            hash *= 1099511628211ull;
        }
        return std::to_wstring(hash);
    }

public:
    void init(HWND gameWindow) {
        createHostWindow(gameWindow);

        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring userDataFolder = std::wstring(tempPath) + cacheFolderName();

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

        bool forced = KeybindRegistry::get().consumeDirty();

        static auto lastUpdate = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (!forced && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 200)
            return;

        lastUpdate = now;

        nlohmann::json j;
        j["state"] = state;
        j["config"] = ConfigManager::get().state;
        j["ui"] = nlohmann::json{ {"collapsed", _collapsed}, {"dock", dockName(_dockEdge)} };
        j["keybinds"] = KeybindRegistry::get().toJson();
        j["capturingId"] = KeybindRegistry::get().capturingId();

        std::string json = j.dump();

        if (json == _lastJson)
            return;

        std::wstring wjson = Utils::toWide(json);
        HRESULT hr = _webview->PostWebMessageAsString(wjson.c_str());
        
        if (SUCCEEDED(hr)) {
            _lastJson = json;
        } else {
            _lastJson.clear();
        }
    }

    void toggleCollapse() {
        if (_panelHwnd)
            PostMessage(_panelHwnd, WM_TOGGLE_COLLAPSE, 0, 0);
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
