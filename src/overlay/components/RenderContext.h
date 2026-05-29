#pragma once
#include <windows.h>
#include <d2d1.h>
#include <string>
#include <cmath>
#include "datastore/GameState.h"

struct RenderContext {
    ID2D1RenderTarget* target = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
    ID2D1PathGeometry* unitDiamond = nullptr;

    int width = 0;
    int height = 0;
    bool valid = false;
    POINT origin{};
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    POINT cursor{};

    void fillDiamond(float cx, float cy, float hw, float hh, const D2D1_COLOR_F& color) const {
        brush->SetColor(color);
        target->SetTransform(D2D1::Matrix3x2F::Scale(hw, hh) * D2D1::Matrix3x2F::Translation(cx, cy));
        target->FillGeometry(unitDiamond, brush);
        target->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    void drawDiamondOutline(float cx, float cy, float hw, float hh, const D2D1_COLOR_F& color, float stroke) const {
        brush->SetColor(color);
        D2D1_POINT_2F left = { cx - hw, cy };
        D2D1_POINT_2F top = { cx, cy - hh };
        D2D1_POINT_2F right = { cx + hw, cy };
        D2D1_POINT_2F bottom = { cx, cy + hh };
        target->DrawLine(left, top, brush, stroke);
        target->DrawLine(top, right, brush, stroke);
        target->DrawLine(right, bottom, brush, stroke);
        target->DrawLine(bottom, left, brush, stroke);
    }

    void fillRect(float left, float top, float right, float bottom, const D2D1_COLOR_F& color) const {
        brush->SetColor(color);
        target->FillRectangle(D2D1::RectF(left, top, right, bottom), brush);
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
