#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace app {
using DriverValues = std::map<std::wstring, uint32_t>;
constexpr uint32_t DriverNoAction = 7;

inline bool MultiFingerField(const std::wstring &name) {
    if (name == L"MultiFingerGesture_Enable")
        return true;
    for (const auto *count : {L"Three", L"Four"}) {
        const std::wstring tap = std::wstring(L"Tap_") + count + L"_Finger";
        const std::wstring move = std::wstring(count) + L"FingerMove";
        if (name == tap || name == tap + L"_Enable" || name == move + L"_Enable")
            return true;
        for (const auto *direction : {L"Up", L"Down", L"Left", L"Right"})
            if (name == move + direction + L"_Func" || name == move + direction + L"_Enable")
                return true;
    }
    return false;
}

// Work only with validated fields supplied by the caller. Missing driver values
// are never invented. Remember the user's choices until the master is enabled.
inline void SetMultiFingerEnabled(DriverValues &values, DriverValues &remembered, bool enabled) {
    if (enabled) {
        for (const auto &entry : remembered)
            if (MultiFingerField(entry.first) && values.count(entry.first))
                values[entry.first] = entry.second;
        remembered.clear();
    } else {
        for (auto &entry : values) {
            if (!MultiFingerField(entry.first))
                continue;
            remembered.emplace(entry.first, entry.second);
            if (entry.first == L"Tap_Three_Finger" || entry.first == L"Tap_Four_Finger")
                entry.second = DriverNoAction;
            else if (entry.first.size() >= 7 &&
                     entry.first.compare(entry.first.size() - 7, 7, L"_Enable") == 0)
                entry.second = 0;
        }
    }
    auto master = values.find(L"MultiFingerGesture_Enable");
    if (master != values.end())
        master->second = enabled ? 1 : 0;
}
} // namespace app
