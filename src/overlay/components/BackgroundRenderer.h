#pragma once
#include "RenderContext.h"
#include "Config.h"

class BackgroundRenderer : public IOverlayComponent {
public:
    bool update(const RenderContext&, const GameState&) override {
        int current = ConfigManager::get().state.backgroundOverlay;
        bool dirty = current != _last;
        _last = current;
        return dirty;
    }

    void render(const RenderContext& ctx, const GameState&) override {
        int overlay = ConfigManager::get().state.backgroundOverlay;
        if (overlay <= 0)
            return;
        ctx.fillRect(0.0f, 0.0f, (float)ctx.width, (float)ctx.height, D2D1::ColorF(0, 0, 0, overlay / 100.0f));
    }

private:
    int _last = 0;
};
