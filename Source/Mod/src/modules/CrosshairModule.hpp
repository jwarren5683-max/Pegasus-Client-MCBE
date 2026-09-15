#pragma once

#include "../framework/Module.hpp"
#include <Windows.h>

namespace utility::modules {

class CrosshairModule final : public Module {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Crosshair"; }
    [[nodiscard]] ModuleCategory category() const noexcept override { return ModuleCategory::visual; }

    void draw_overlay(void* context, int width, int height) noexcept override {
        if (context == nullptr || width <= 0 || height <= 0) return;
        const auto dc = static_cast<HDC>(context);
        const int x = width / 2;
        const int y = height / 2;
        const auto pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        if (pen == nullptr) return;
        const auto old = SelectObject(dc, pen);
        MoveToEx(dc, x - 8, y, nullptr); LineTo(dc, x - 2, y);
        MoveToEx(dc, x + 2, y, nullptr); LineTo(dc, x + 8, y);
        MoveToEx(dc, x, y - 8, nullptr); LineTo(dc, x, y - 2);
        MoveToEx(dc, x, y + 2, nullptr); LineTo(dc, x, y + 8);
        SelectObject(dc, old);
        DeleteObject(pen);
    }
};

} // namespace utility::modules
