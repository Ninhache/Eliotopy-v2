#pragma once
#include <windows.h>
#include <string>
#include <random>
#include <chrono>
#include <cmath>
#include "datastore/GameState.h"
#include "Config.h"

class Renderer {
private:
    HWND _targetHwnd = nullptr;
    HWND _overlayHwnd = nullptr;
    HWND _bgOverlayHwnd = nullptr;
    bool _initialized = false;
    std::string _className;

    HDC _cachedDC = nullptr;
    HBITMAP _cachedBitmap = nullptr;
    HPEN _gridPen = nullptr;
    HBRUSH _bgBrush = nullptr;
    int _cachedWidth = 0;
    int _cachedHeight = 0;
    glm::mat4 _lastMatrix = glm::mat4(0.0f);
    size_t _lastCellCount = 0;
    std::string _lastGridColor = "";
    std::string _lastGridFillColor = "";
    int _lastGridFillOpacity = 60;
    int _lastBackgroundOverlay = 0;

    COLORREF parseHexColor(const std::string& hexStr) {
        if (hexStr.length() >= 7 && hexStr[0] == '#') {
            try {
                int r = std::stoi(hexStr.substr(1, 2), nullptr, 16);
                int g = std::stoi(hexStr.substr(3, 2), nullptr, 16);
                int b = std::stoi(hexStr.substr(5, 2), nullptr, 16);
                return RGB(r, g, b);
            } catch (...) {}
        }
        return RGB(255, 255, 255);
    }

    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_PAINT) {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        if (uMsg == WM_ERASEBKGND)
            return 1;
            
        if (uMsg == WM_SETCURSOR) {
            SetCursor(NULL);
            return TRUE;
        }
        
        if (uMsg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        
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

    bool WorldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj, int screenWidth, int screenHeight, POINT& outPos) {
        glm::vec4 clipCoords = viewProj * glm::vec4(worldPos, 1.0f);
        if (clipCoords.w < 0.1f && clipCoords.w > -0.1f)
            return false;
            
        glm::vec3 ndc = glm::vec3(clipCoords) / clipCoords.w;
        outPos.x = (LONG)((screenWidth / 2.0f) * (ndc.x + 1.0f));
        outPos.y = (LONG)((screenHeight / 2.0f) * (1.0f - ndc.y));
        return true;
    }

    void ensureBackBuffer(HDC screenDC, int w, int h) {
        if (_cachedDC && _cachedWidth == w && _cachedHeight == h)
            return;
            
        if (_cachedBitmap)
            DeleteObject(_cachedBitmap);
            
        if (_cachedDC)
            DeleteDC(_cachedDC);
            
        _cachedDC = CreateCompatibleDC(screenDC);
        _cachedBitmap = CreateCompatibleBitmap(screenDC, w, h);
        SelectObject(_cachedDC, _cachedBitmap);
        _cachedWidth = w;
        _cachedHeight = h;
    }
    HBRUSH createTransparentBrush(COLORREF color, int opacityPercent) {
        int pixelsToColor = (opacityPercent * 64) / 100;
        
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = 8;
        bmi.bmiHeader.biHeight = -8;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        void* bits = nullptr;
        HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (hbmp && bits) {
            DWORD* pixels = static_cast<DWORD*>(bits);
            
            const int bayer8x8[64] = {
                 0, 32,  8, 40,  2, 34, 10, 42,
                48, 16, 56, 24, 50, 18, 58, 26,
                12, 44,  4, 36, 14, 46,  6, 38,
                60, 28, 52, 20, 62, 30, 54, 22,
                 3, 35, 11, 43,  1, 33,  9, 41,
                51, 19, 59, 27, 49, 17, 57, 25,
                15, 47,  7, 39, 13, 45,  5, 37,
                63, 31, 55, 23, 61, 29, 53, 21
            };
            
            DWORD transparentColor = RGB(255, 0, 255);
            DWORD targetColorRaw = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
            DWORD transColorRaw  = (GetRValue(transparentColor) << 16) | (GetGValue(transparentColor) << 8) | GetBValue(transparentColor);
            
            for (int i = 0; i < 64; ++i) {
                if (bayer8x8[i] < pixelsToColor) {
                    pixels[i] = targetColorRaw;
                } else {
                    pixels[i] = transColorRaw;
                }
            }
            
            HBRUSH brush = CreatePatternBrush(hbmp);
            DeleteObject(hbmp);
            return brush;
        }
        return CreateSolidBrush(color);
    }

    void renderGrid(int winWidth, int winHeight, const GameState& state) {
        if (!state.isInGame || state.cells.empty())
            return;
            
        bool gridOverlay = ConfigManager::get().state.gridOverlay;
        if (!gridOverlay && state.trackedEntityId == 0)
            return;

        HGDIOBJ oldPen = SelectObject(_cachedDC, _gridPen);
        
        POINT origin, px, py;
        if (WorldToScreen(glm::vec3(0, 0, 0), state.viewProjMatrix, winWidth, winHeight, origin) &&
            WorldToScreen(glm::vec3(1000, 0, 0), state.viewProjMatrix, winWidth, winHeight, px) &&
            WorldToScreen(glm::vec3(0, 1000, 0), state.viewProjMatrix, winWidth, winHeight, py)) 
        {
            float scaleX = (px.x - origin.x) / 1000.0f;
            float scaleY = (py.y - origin.y) / 1000.0f; 

            HPEN losPen = (HPEN)GetStockObject(NULL_PEN);
            COLORREF fillColor = parseHexColor(ConfigManager::get().state.gridFillColor);
            int fillOpacity = ConfigManager::get().state.gridFillOpacity;
            HBRUSH losBrush = createTransparentBrush(fillColor, fillOpacity);
            std::string gridMode = ConfigManager::get().state.gridMode;
            bool showLos = ConfigManager::get().state.gridShowLos;
            bool showMove = ConfigManager::get().state.gridShowMove;

            int32_t trackedCellId = -1;
            if (state.trackedEntityId != 0) {
                if (state.trackedEntityId == state.player.id) {
                    trackedCellId = state.player.cellId;
                } else if (state.mapEntities.contains(state.trackedEntityId)) {
                    trackedCellId = state.mapEntities.at(state.trackedEntityId).cellId;
                } else if (state.fightEntities.contains(state.trackedEntityId)) {
                    trackedCellId = state.fightEntities.at(state.trackedEntityId).cellId;
                } else if (state.portalEntities.contains(state.trackedEntityId)) {
                    trackedCellId = state.portalEntities.at(state.trackedEntityId).cellId;
                }
            }

            for (const auto& cell : state.cells) {
                bool shouldDraw = false;
                bool isLosBlocked = false;

                if (gridMode == "rp") {
                    shouldDraw = cell.mov && !cell.nonWalkableDuringRP;
                } else if (gridMode == "fight") {
                    shouldDraw = cell.mov && !cell.nonWalkableDuringFight;
                }

                if (showMove) {
                    if (cell.mov) shouldDraw = true;
                }
                
                if (showLos) {
                    if (!cell.los) {
                        shouldDraw = true;
                        isLosBlocked = true;
                    }
                }

                if (!gridOverlay) {
                    shouldDraw = false;
                    isLosBlocked = false;
                }

                bool isTrackedCell = (trackedCellId != -1 && cell.cellNumber == trackedCellId);

                if (!shouldDraw && !isTrackedCell) continue;

                float cx = origin.x + cell.x * scaleX;
                float cy = origin.y + cell.y * scaleY;
                
                float hw = std::abs((cell.width / 2.0f) * scaleX);
                float hh = std::abs((cell.height / 2.0f) * scaleY);
                
                POINT pts[5];
                pts[0] = { (LONG)(cx - hw), (LONG)cy };
                pts[1] = { (LONG)cx,        (LONG)(cy - hh) };
                pts[2] = { (LONG)(cx + hw), (LONG)cy };
                pts[3] = { (LONG)cx,        (LONG)(cy + hh) };
                pts[4] = pts[0];

                if (shouldDraw) {
                    if (isLosBlocked) {
                        SelectObject(_cachedDC, losPen);
                        SelectObject(_cachedDC, losBrush);
                        Polygon(_cachedDC, pts, 4);
                    } else {
                        SelectObject(_cachedDC, _gridPen);
                        SelectObject(_cachedDC, GetStockObject(NULL_BRUSH));
                        Polyline(_cachedDC, pts, 5);
                    }
                }

                if (isTrackedCell) {
                    bool blinkOn = ((GetTickCount64() / 250) % 2) == 0;
                    
                    HPEN trackPen = CreatePen(PS_SOLID, 2, RGB(180, 0, 0));
                    SelectObject(_cachedDC, trackPen);
                    
                    if (blinkOn) {
                        HBRUSH trackBrush = CreateSolidBrush(RGB(180, 0, 0));
                        SelectObject(_cachedDC, trackBrush);
                        Polygon(_cachedDC, pts, 4);
                        DeleteObject(trackBrush);
                    } else {
                        SelectObject(_cachedDC, GetStockObject(NULL_BRUSH));
                        Polygon(_cachedDC, pts, 4);
                    }
                    DeleteObject(trackPen);
                }
            }
            DeleteObject(losBrush);
        }
        SelectObject(_cachedDC, oldPen);
    }

public:
    void init(HWND target) {
        _targetHwnd = target;
        _className = GenerateRandomString(12);
        
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = _className.c_str();
        wc.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
        
        RegisterClassExA(&wc);

        RECT clientRect;
        GetClientRect(_targetHwnd, &clientRect);
        POINT tl = { clientRect.left, clientRect.top };
        POINT br = { clientRect.right, clientRect.bottom };
        ClientToScreen(_targetHwnd, &tl);
        ClientToScreen(_targetHwnd, &br);

        _bgOverlayHwnd = CreateWindowExA(
            WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            _className.c_str(),
            "",
            WS_POPUP,
            tl.x, tl.y, br.x - tl.x, br.y - tl.y,
            _targetHwnd, nullptr, wc.hInstance, nullptr
        );

        _overlayHwnd = CreateWindowExA(
            WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            _className.c_str(),
            "",
            WS_POPUP,
            tl.x, tl.y, br.x - tl.x, br.y - tl.y,
            _targetHwnd, nullptr, wc.hInstance, nullptr
        );

        SetLayeredWindowAttributes(_overlayHwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
        ShowWindow(_overlayHwnd, SW_SHOW);

        int bgAlpha = (ConfigManager::get().state.backgroundOverlay * 255) / 100;
        SetLayeredWindowAttributes(_bgOverlayHwnd, 0, bgAlpha, LWA_ALPHA);
        ShowWindow(_bgOverlayHwnd, bgAlpha > 0 ? SW_SHOW : SW_HIDE);
        if (bgAlpha > 0) {
            HDC bgDc = GetDC(_bgOverlayHwnd);
            RECT br; GetClientRect(_bgOverlayHwnd, &br);
            FillRect(bgDc, &br, (HBRUSH)GetStockObject(BLACK_BRUSH));
            ReleaseDC(_bgOverlayHwnd, bgDc);
        }

        SetWindowPos(_bgOverlayHwnd, _overlayHwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        _lastGridColor = ConfigManager::get().state.gridColor;
        _lastGridFillColor = ConfigManager::get().state.gridFillColor;
        _lastGridFillOpacity = ConfigManager::get().state.gridFillOpacity;
        _lastBackgroundOverlay = ConfigManager::get().state.backgroundOverlay;
        _gridPen = CreatePen(PS_SOLID, 1, parseHexColor(_lastGridColor));
        _bgBrush = CreateSolidBrush(RGB(255, 0, 255));

        _initialized = true;
    }

    void update(const GameState& state) {
        if (!_initialized)
            return;

        static auto lastDraw = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDraw).count() < 33)
            return;
            
        lastDraw = now;

        RECT clientRect;
        if (GetClientRect(_targetHwnd, &clientRect)) {
            POINT tl = { clientRect.left, clientRect.top };
            POINT br = { clientRect.right, clientRect.bottom };
            ClientToScreen(_targetHwnd, &tl);
            ClientToScreen(_targetHwnd, &br);
            int width = br.x - tl.x;
            int height = br.y - tl.y;
            SetWindowPos(_overlayHwnd, NULL, tl.x, tl.y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowPos(_bgOverlayHwnd, _overlayHwnd, tl.x, tl.y, width, height, SWP_NOACTIVATE);
        }

        RECT overlayRect;
        GetClientRect(_overlayHwnd, &overlayRect);
        int winWidth = overlayRect.right - overlayRect.left;
        int winHeight = overlayRect.bottom - overlayRect.top;
        
        bool matrixChanged = (state.viewProjMatrix != _lastMatrix);
        bool cellCountChanged = (state.cells.size() != _lastCellCount);
        bool sizeChanged = (winWidth != _cachedWidth || winHeight != _cachedHeight);
        
        static uint32_t _lastMapIdRenderer = 0;
        bool mapIdChanged = (state.mapInfo.id != _lastMapIdRenderer);
        if (mapIdChanged) _lastMapIdRenderer = state.mapInfo.id;
        
        static bool _lastGridOverlay = true;
        bool gridOverlayChanged = (ConfigManager::get().state.gridOverlay != _lastGridOverlay);

        static std::string _lastGridMode = "rp";
        bool gridModeChanged = (ConfigManager::get().state.gridMode != _lastGridMode);
        
        static bool _lastShowLos = false;
        bool showLosChanged = (ConfigManager::get().state.gridShowLos != _lastShowLos);

        static bool _lastShowMove = false;
        bool showMoveChanged = (ConfigManager::get().state.gridShowMove != _lastShowMove);
        
        bool fillColorChanged = (ConfigManager::get().state.gridFillColor != _lastGridFillColor);
        bool fillOpacityChanged = (ConfigManager::get().state.gridFillOpacity != _lastGridFillOpacity);
        
        bool bgOverlayChanged = (ConfigManager::get().state.backgroundOverlay != _lastBackgroundOverlay);

        if (bgOverlayChanged || sizeChanged) {
            _lastBackgroundOverlay = ConfigManager::get().state.backgroundOverlay;
            int bgAlpha = (_lastBackgroundOverlay * 255) / 100;
            SetLayeredWindowAttributes(_bgOverlayHwnd, 0, bgAlpha, LWA_ALPHA);
            ShowWindow(_bgOverlayHwnd, bgAlpha > 0 ? SW_SHOW : SW_HIDE);
            if (bgAlpha > 0) {
                HDC bgDc = GetDC(_bgOverlayHwnd);
                RECT br; GetClientRect(_bgOverlayHwnd, &br);
                FillRect(bgDc, &br, (HBRUSH)GetStockObject(BLACK_BRUSH));
                ReleaseDC(_bgOverlayHwnd, bgDc);
            }
        }

        bool needsRedraw = mapIdChanged || matrixChanged || cellCountChanged || gridOverlayChanged || gridModeChanged || showLosChanged || showMoveChanged || fillColorChanged || fillOpacityChanged || sizeChanged;

        static int64_t _lastTrackedEntityId = 0;
        if (state.trackedEntityId != _lastTrackedEntityId) {
            needsRedraw = true;
            _lastTrackedEntityId = state.trackedEntityId;
        }
        
        if (state.trackedEntityId != 0) {
            needsRedraw = true;
        }

        if (!needsRedraw) {
            if (std::abs((int)state.cells.size() - (int)_lastCellCount) > 5) {
                needsRedraw = true;
                _lastCellCount = state.cells.size();
            }
        }

        if (ConfigManager::get().state.gridColor != _lastGridColor) {
            _lastGridColor = ConfigManager::get().state.gridColor;
            if (_gridPen) DeleteObject(_gridPen);
            _gridPen = CreatePen(PS_SOLID, 1, parseHexColor(_lastGridColor));
            needsRedraw = true;
        }

        if (!needsRedraw)
            return;

        _lastMatrix = state.viewProjMatrix;
        _lastCellCount = state.cells.size();
        _lastGridOverlay = ConfigManager::get().state.gridOverlay;
        _lastGridMode = ConfigManager::get().state.gridMode;
        _lastShowLos = ConfigManager::get().state.gridShowLos;
        _lastShowMove = ConfigManager::get().state.gridShowMove;
        _lastGridFillColor = ConfigManager::get().state.gridFillColor;
        _lastGridFillOpacity = ConfigManager::get().state.gridFillOpacity;

        HDC hdc = GetDC(_overlayHwnd);

        if (winWidth <= 0 || winHeight <= 0) {
            ReleaseDC(_overlayHwnd, hdc);
            return;
        }

        ensureBackBuffer(hdc, winWidth, winHeight);
        FillRect(_cachedDC, &overlayRect, _bgBrush);

        renderGrid(winWidth, winHeight, state);
        BitBlt(hdc, 0, 0, winWidth, winHeight, _cachedDC, 0, 0, SRCCOPY);
        ReleaseDC(_overlayHwnd, hdc);
        InvalidateRect(_overlayHwnd, NULL, FALSE);
    }

    ~Renderer() {
        if (_gridPen)
            DeleteObject(_gridPen);
            
        if (_bgBrush)
            DeleteObject(_bgBrush);
            
        if (_cachedBitmap)
            DeleteObject(_cachedBitmap);
            
        if (_cachedDC)
            DeleteDC(_cachedDC);
    }
};
