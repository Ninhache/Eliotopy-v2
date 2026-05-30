#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <chrono>
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include "datastore/GameState.h"
#include "Config.h"
#include "Utils.h"
#include "components/RenderContext.h"
#include "components/BackgroundRenderer.h"
#include "components/GridRenderer.h"
#include "components/LosRenderer.h"
#include "components/PortalRenderer.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

class Renderer {
private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    HWND _targetHwnd = nullptr;
    HWND _overlayHwnd = nullptr;
    bool _initialized = false;
    std::string _className;

    ComPtr<ID2D1Factory> _d2dFactory;
    ComPtr<ID2D1DCRenderTarget> _renderTarget;
    ComPtr<ID2D1SolidColorBrush> _brush;
    ComPtr<ID2D1PathGeometry> _unitDiamond;
    ComPtr<ID2D1StrokeStyle> _miterStroke;
    ComPtr<IDWriteFactory> _dwriteFactory;
    ComPtr<IDWriteTextFormat> _textFormat;

    HDC _memDC = nullptr;
    HBITMAP _dibBitmap = nullptr;
    HBITMAP _oldBitmap = nullptr;
    void* _dibBits = nullptr;

    int _cachedWidth = 0;
    int _cachedHeight = 0;
    RECT _lastRect = {};
    glm::mat4 _lastMatrix = glm::mat4(0.0f);
    size_t _lastCellCount = 0;
    uint32_t _lastMapId = 0;

    std::unordered_map<int, const Cell*> _cellById;
    std::vector<std::unique_ptr<IOverlayComponent>> _components;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_NCHITTEST:
                return HTTRANSPARENT;
            case WM_ERASEBKGND:
                return 1;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }

    static bool worldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj, int screenWidth, int screenHeight, POINT& outPos) {
        glm::vec4 clipCoords = viewProj * glm::vec4(worldPos, 1.0f);
        if (clipCoords.w < 0.1f && clipCoords.w > -0.1f)
            return false;

        glm::vec3 ndc = glm::vec3(clipCoords) / clipCoords.w;
        outPos.x = (LONG)((screenWidth / 2.0f) * (ndc.x + 1.0f));
        outPos.y = (LONG)((screenHeight / 2.0f) * (1.0f - ndc.y));
        return true;
    }

    static bool computeBasis(const glm::mat4& matrix, int winWidth, int winHeight, POINT& origin, float& scaleX, float& scaleY) {
        POINT px, py;
        if (!worldToScreen(glm::vec3(0, 0, 0), matrix, winWidth, winHeight, origin) ||
            !worldToScreen(glm::vec3(1000, 0, 0), matrix, winWidth, winHeight, px) ||
            !worldToScreen(glm::vec3(0, 1000, 0), matrix, winWidth, winHeight, py))
            return false;
        scaleX = (px.x - origin.x) / 1000.0f;
        scaleY = (py.y - origin.y) / 1000.0f;
        return true;
    }

    void buildCellMap(const GameState& state) {
        _cellById.clear();
        _cellById.reserve(state.cells.size() * 2);
        for (const auto& cell : state.cells)
            _cellById[cell.cellNumber] = &cell;
    }

    int nearestCell(const GameState& state, const RenderContext& ctx) const {
        if (!ctx.valid || state.cells.empty())
            return -1;
        int best = -1;
        double bestDist = 1e18;
        for (const auto& cell : state.cells) {
            double dx = ctx.cursor.x - ctx.cellX(cell);
            double dy = ctx.cursor.y - ctx.cellY(cell);
            double dist = dx * dx + dy * dy;
            if (dist < bestDist) {
                bestDist = dist;
                best = cell.cellNumber;
            }
        }
        return best;
    }

    bool createGraphics() {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, _d2dFactory.GetAddressOf())))
            return false;

        if (!createRenderTarget())
            return false;

        if (!createUnitDiamond())
            return false;

        if (!createStrokeStyles())
            return false;

        return createTextFormat();
    }

    bool createTextFormat() {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown**>(_dwriteFactory.GetAddressOf()))))
            return false;

        if (FAILED(_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
                                                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                    14.0f, L"en-us", _textFormat.GetAddressOf())))
            return false;

        _textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        _textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        return true;
    }

    bool createRenderTarget() {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, 96.0f);

        if (FAILED(_d2dFactory->CreateDCRenderTarget(&props, _renderTarget.ReleaseAndGetAddressOf())))
            return false;

        return SUCCEEDED(_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), _brush.ReleaseAndGetAddressOf()));
    }

    bool createUnitDiamond() {
        if (FAILED(_d2dFactory->CreatePathGeometry(&_unitDiamond)))
            return false;

        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(_unitDiamond->Open(&sink)))
            return false;

        static constexpr D2D1_POINT_2F edges[] = { { 0.0f, -1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } };
        sink->BeginFigure(D2D1::Point2F(-1.0f, 0.0f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLines(edges, ARRAYSIZE(edges));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        return SUCCEEDED(sink->Close());
    }

    bool createStrokeStyles() {
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
            D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f);
        return SUCCEEDED(_d2dFactory->CreateStrokeStyle(props, nullptr, 0, _miterStroke.ReleaseAndGetAddressOf()));
    }

    bool ensureSurface(int width, int height) {
        if (width <= 0 || height <= 0)
            return false;

        if (_dibBitmap && _cachedWidth == width && _cachedHeight == height)
            return true;

        if (!_memDC) {
            HDC screenDC = GetDC(nullptr);
            _memDC = CreateCompatibleDC(screenDC);
            ReleaseDC(nullptr, screenDC);
            if (!_memDC)
                return false;
        }

        if (_oldBitmap) {
            SelectObject(_memDC, _oldBitmap);
            _oldBitmap = nullptr;
        }
        if (_dibBitmap) {
            DeleteObject(_dibBitmap);
            _dibBitmap = nullptr;
            _dibBits = nullptr;
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        _dibBitmap = CreateDIBSection(_memDC, &bmi, DIB_RGB_COLORS, &_dibBits, nullptr, 0);
        if (!_dibBitmap)
            return false;

        _oldBitmap = (HBITMAP)SelectObject(_memDC, _dibBitmap);

        RECT rc = { 0, 0, width, height };
        if (FAILED(_renderTarget->BindDC(_memDC, &rc)))
            return false;

        _cachedWidth = width;
        _cachedHeight = height;
        return true;
    }

    bool getTargetRect(RECT& out) const {
        RECT clientRect;
        if (!GetClientRect(_targetHwnd, &clientRect))
            return false;

        POINT tl = { clientRect.left, clientRect.top };
        POINT br = { clientRect.right, clientRect.bottom };
        ClientToScreen(_targetHwnd, &tl);
        ClientToScreen(_targetHwnd, &br);

        out = { tl.x, tl.y, br.x, br.y };
        return (br.x > tl.x && br.y > tl.y);
    }

    void present(const RECT& rect, int width, int height) {
        POINT ptSrc = { 0, 0 };
        POINT ptDst = { rect.left, rect.top };
        SIZE size = { width, height };
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

        HDC screenDC = GetDC(nullptr);
        UpdateLayeredWindow(_overlayHwnd, screenDC, &ptDst, &size, _memDC, &ptSrc, 0, &blend, ULW_ALPHA);
        ReleaseDC(nullptr, screenDC);
    }

public:
    void init(HWND target) {
        _targetHwnd = target;
        _className = Utils::randomString(12);

        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = _className.c_str();
        RegisterClassExA(&wc);

        RECT clientRect;
        GetClientRect(_targetHwnd, &clientRect);
        POINT tl = { clientRect.left, clientRect.top };
        POINT br = { clientRect.right, clientRect.bottom };
        ClientToScreen(_targetHwnd, &tl);
        ClientToScreen(_targetHwnd, &br);

        _overlayHwnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            _className.c_str(),
            "",
            WS_POPUP,
            tl.x, tl.y, br.x - tl.x, br.y - tl.y,
            _targetHwnd, nullptr, wc.hInstance, nullptr
        );

        if (!_overlayHwnd)
            return;

        if (!createGraphics()) {
            Logger::error("Failed to initialize Direct2D renderer");
            return;
        }

        _components.push_back(std::make_unique<BackgroundRenderer>());
        _components.push_back(std::make_unique<GridRenderer>());
        _components.push_back(std::make_unique<PortalRenderer>());
        _components.push_back(std::make_unique<LosRenderer>());

        ShowWindow(_overlayHwnd, SW_SHOWNA);
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

        if (!IsWindow(_targetHwnd))
            return;

        if (IsIconic(_targetHwnd)) {
            if (IsWindowVisible(_overlayHwnd))
                ShowWindow(_overlayHwnd, SW_HIDE);
            return;
        }
        if (!IsWindowVisible(_overlayHwnd))
            ShowWindow(_overlayHwnd, SW_SHOWNA);

        RECT rect;
        if (!getTargetRect(rect))
            return;

        int winWidth = rect.right - rect.left;
        int winHeight = rect.bottom - rect.top;

        if (!ensureSurface(winWidth, winHeight))
            return;

        RenderContext ctx;
        ctx.target = _renderTarget.Get();
        ctx.brush = _brush.Get();
        ctx.unitDiamond = _unitDiamond.Get();
        ctx.miterStroke = _miterStroke.Get();
        ctx.width = winWidth;
        ctx.height = winHeight;
        ctx.valid = computeBasis(state.viewProjMatrix, winWidth, winHeight, ctx.origin, ctx.scaleX, ctx.scaleY);
        ctx.factory = _d2dFactory.Get();
        ctx.dwrite = _dwriteFactory.Get();
        ctx.textFormat = _textFormat.Get();
        GetCursorPos(&ctx.cursor);
        ScreenToClient(_targetHwnd, &ctx.cursor);

        buildCellMap(state);
        ctx.cellById = &_cellById;
        ctx.hoveredCell = nearestCell(state, ctx);

        bool rectChanged = (rect.left != _lastRect.left || rect.top != _lastRect.top ||
                            rect.right != _lastRect.right || rect.bottom != _lastRect.bottom);
        bool matrixChanged = (state.viewProjMatrix != _lastMatrix);
        bool cellCountChanged = (state.cells.size() != _lastCellCount);
        bool mapIdChanged = (state.mapInfo.id != _lastMapId);

        _lastRect = rect;
        _lastMatrix = state.viewProjMatrix;
        _lastCellCount = state.cells.size();
        _lastMapId = state.mapInfo.id;

        bool dirty = rectChanged || matrixChanged || cellCountChanged || mapIdChanged;
        for (auto& component : _components)
            dirty |= component->update(ctx, state);

        if (!dirty)
            return;

        _renderTarget->BeginDraw();
        _renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

        for (auto& component : _components)
            component->render(ctx, state);

        if (_renderTarget->EndDraw() == D2DERR_RECREATE_TARGET) {
            createRenderTarget();
            _cachedWidth = 0;
            _cachedHeight = 0;
            _lastRect = {};
            return;
        }

        present(rect, winWidth, winHeight);
    }

    ~Renderer() {
        if (_memDC) {
            if (_oldBitmap)
                SelectObject(_memDC, _oldBitmap);
            DeleteDC(_memDC);
        }
        if (_dibBitmap)
            DeleteObject(_dibBitmap);
    }
};
