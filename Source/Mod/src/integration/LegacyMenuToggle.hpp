#pragma once

namespace utility::integration {

class LegacyMenuToggle final {
public:
    [[nodiscard]] bool visible() const noexcept { return visible_; }

    bool update(unsigned key, bool down, bool gameplay) noexcept {
        if (key != 'C') return false;
        if (!down) {
            held_ = false;
            return false;
        }
        if (held_) return false;
        held_ = true;
        if (!gameplay) return false;
        visible_ = !visible_;
        return true;
    }

private:
    bool visible_{true};
    bool held_{};
};

} // namespace utility::integration
