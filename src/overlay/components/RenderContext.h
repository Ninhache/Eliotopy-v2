#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <cmath>
#include <unordered_map>
#include "datastore/GameState.h"

struct RenderContext {
    ID2D1RenderTarget* target = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
    ID2D1PathGeometry* unitDiamond = nullptr;
    ID2D1StrokeStyle* miterStroke = nullptr;
    ID2D1Factory* factory = nullptr;
    IDWriteFactory* dwrite = nullptr;
    IDWriteTextFormat* textFormat = nullptr;

    int width = 0;
    int height = 0;
    bool valid = false;
    POINT origin{};
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    POINT cursor{};
    int hoveredCell = -1;
    const std::unordered_map<int, const Cell*>* cellById = nullptr;

    void drawLine(float x1, float y1, float x2, float y2, const D2D1_COLOR_F& color, float stroke) const {
        brush->SetColor(color);
        target->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush, stroke);
    }

    void drawTextCentered(const std::wstring& text, float cx, float cy, const D2D1_COLOR_F& color) const {
        if (!textFormat || !dwrite || text.empty())
            return;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        if (FAILED(dwrite->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), textFormat, 160.0f, 32.0f, layout.GetAddressOf())))
            return;
        brush->SetColor(color);
        target->DrawTextLayout(D2D1::Point2F(cx - 80.0f, cy - 16.0f), layout.Get(), brush);
    }

    void fillDiamond(float cx, float cy, float hw, float hh, const D2D1_COLOR_F& color) const {
        brush->SetColor(color);
        target->SetTransform(D2D1::Matrix3x2F::Scale(hw, hh) * D2D1::Matrix3x2F::Translation(cx, cy));
        target->FillGeometry(unitDiamond, brush);
        target->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    void drawDiamondOutline(float cx, float cy, float hw, float hh, const D2D1_COLOR_F& color, float stroke) const {
        if (!factory)
            return;
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
        if (FAILED(factory->CreatePathGeometry(geometry.GetAddressOf())))
            return;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(geometry->Open(sink.GetAddressOf())))
            return;
        const D2D1_POINT_2F edges[] = { { cx, cy - hh }, { cx + hw, cy }, { cx, cy + hh } };
        sink->BeginFigure(D2D1::Point2F(cx - hw, cy), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLines(edges, ARRAYSIZE(edges));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        if (FAILED(sink->Close()))
            return;
        brush->SetColor(color);
        target->DrawGeometry(geometry.Get(), brush, stroke, miterStroke);
    }

    void fillRect(float left, float top, float right, float bottom, const D2D1_COLOR_F& color) const {
        brush->SetColor(color);
        target->FillRectangle(D2D1::RectF(left, top, right, bottom), brush);
    }

    void fillCircle(float cx, float cy, float radius, const D2D1_COLOR_F& color) const {
        brush->SetColor(color);
        target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), brush);
    }

    void fillRoundedRect(float cx, float cy, float halfW, float halfH, float radius, const D2D1_COLOR_F& color) const {
        brush->SetColor(color);
        target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - halfW, cy - halfH, cx + halfW, cy + halfH), radius, radius), brush);
    }

    void drawTextScaled(const std::wstring& text, float cx, float cy, float fontSize, const D2D1_COLOR_F& color, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_BOLD) const {
        if (!textFormat || !dwrite || text.empty())
            return;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        if (FAILED(dwrite->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), textFormat, 200.0f, 80.0f, layout.GetAddressOf())))
            return;
        DWRITE_TEXT_RANGE range{ 0, static_cast<UINT32>(text.size()) };
        layout->SetFontSize(fontSize, range);
        layout->SetFontWeight(weight, range);
        brush->SetColor(color);
        target->DrawTextLayout(D2D1::Point2F(cx - 100.0f, cy - 40.0f), layout.Get(), brush);
    }

    float cellX(const Cell& cell) const { return origin.x + cell.x * scaleX; }
    float cellY(const Cell& cell) const { return origin.y + cell.y * scaleY; }
    float cellHalfW(const Cell& cell) const { return std::abs((cell.width * 0.5f) * scaleX); }
    float cellHalfH(const Cell& cell) const { return std::abs((cell.height * 0.5f) * scaleY); }

    static D2D1_COLOR_F parseHexColor(const std::string& hexStr, float alpha) {
        int r = 255, g = 255, b = 255;
        if (hexStr.length() >= 7 && hexStr[0] == '#') {
            try {
                r = std::stoi(hexStr.substr(1, 2), nullptr, 16);
                g = std::stoi(hexStr.substr(3, 2), nullptr, 16);
                b = std::stoi(hexStr.substr(5, 2), nullptr, 16);
            } catch (...) {}
        }
        return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, alpha);
    }
};

class IOverlayComponent {
public:
    virtual ~IOverlayComponent() = default;
    virtual bool update(const RenderContext& ctx, const GameState& state) = 0;
    virtual void render(const RenderContext& ctx, const GameState& state) = 0;
};
