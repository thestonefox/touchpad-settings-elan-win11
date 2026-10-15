#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace app {
struct Contact {
    bool down = false;
    float x = 0, y = 0;
};
enum FiveAction {
    FiveOff,
    FiveDesktop,
    FiveTaskView,
    FivePreviousDesktop,
    FiveNextDesktop,
    FivePlayPause,
    FiveMute,
    FiveVolumeUp,
    FiveVolumeDown,
    FivePreviousTrack,
    FiveNextTrack,
    FiveActionCount
};
struct FiveConfig {
    bool enabled = false;
    // Tap, up, down, left, right. Default is entirely inactive.
    std::array<int, 5> actions{};
    bool swipesEnabled = true;
    bool operator==(const FiveConfig &o) const {
        return enabled == o.enabled && actions == o.actions && swipesEnabled == o.swipesEnabled;
    }
    bool operator!=(const FiveConfig &o) const {
        return !(*this == o);
    }
};
// Pure recognizer: no Windows APIs, registry access or input injection.
// It requires all five contacts, waits for stability and fires once, only
// when ALL fingers are lifted. A partial lift cannot become a second gesture.
class FiveFingerRecognizer {
    bool tracking = false, blocked = false;
    uint64_t start = 0, partial = 0;
    std::array<Contact, 5> origin{};
    float maxTravel = 0;
    int direction = -1;

  public:
    void Reset() {
        tracking = false;
        blocked = false;
        start = partial = 0;
        maxTravel = 0;
        direction = -1;
    }
    int Feed(const std::array<Contact, 5> &points, uint64_t now, const FiveConfig &cfg) {
        int count = 0;
        for (auto &p : points)
            if (p.down)
                ++count;
        if (!cfg.enabled) {
            Reset();
            return FiveOff;
        }
        if (blocked) {
            if (!count)
                Reset();
            return FiveOff;
        }
        if (!tracking) {
            if (count == 5) {
                tracking = true;
                start = now;
                origin = points;
                maxTravel = 0;
                direction = -1;
                partial = 0;
            }
            return FiveOff;
        }
        uint64_t duration = now - start;
        if (!count) {
            int slot = direction;
            if (slot < 0 && duration >= 100 && duration <= 550 && maxTravel < .035f)
                slot = 0;
            int action = slot >= 0 && duration <= 1600 ? cfg.actions[slot] : FiveOff;
            if (slot > 0 && !cfg.swipesEnabled)
                action = FiveOff;
            Reset();
            return action;
        }
        if (duration > 1600) {
            blocked = true;
            return FiveOff;
        }
        if (count != 5) {
            if (!partial)
                partial = now;
            if (now - partial > 220)
                blocked = true;
            return FiveOff;
        }
        if (partial) {
            blocked = true;
            return FiveOff;
        } // Re-touch after a partial lift is not a new gesture.
        float dx = 0, dy = 0;
        for (int i = 0; i < 5; ++i) {
            float x = points[i].x - origin[i].x, y = points[i].y - origin[i].y;
            dx += x;
            dy += y;
            maxTravel = std::max(maxTravel, std::sqrt(x * x + y * y));
        }
        dx /= 5;
        dy /= 5;
        if (duration < 100 || direction >= 0)
            return FiveOff;
        float ax = std::fabs(dx), ay = std::fabs(dy);
        if (std::max(ax, ay) < .11f)
            return FiveOff;
        bool horizontal = ax > ay * 1.35f, vertical = ay > ax * 1.35f;
        if (!horizontal && !vertical)
            return FiveOff;
        // Reject spread/pinch motion: all five contacts must move together.
        for (int i = 0; i < 5; ++i) {
            float a = horizontal ? points[i].x - origin[i].x : points[i].y - origin[i].y;
            float mean = horizontal ? dx : dy;
            if (a * mean <= 0 || std::fabs(a) < std::fabs(mean) * .35f)
                return FiveOff;
        }
        direction = horizontal ? (dx < 0 ? 3 : 4) : (dy < 0 ? 1 : 2);
        return FiveOff;
    }
};
} // namespace app
