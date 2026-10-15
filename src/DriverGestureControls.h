#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace app {
using DriverValues = std::map<std::wstring, uint32_t>;
constexpr uint32_t DriverNoAction = 7;

inline bool FingerGroupField(const std::wstring &name, int fingers) {
    if (fingers != 3 && fingers != 4)
        return false;
    const std::wstring count = fingers == 3 ? L"Three" : L"Four";
    const std::wstring tap = L"Tap_" + count + L"_Finger";
    const std::wstring move = count + L"FingerMove";
    if (name == tap || name == tap + L"_Enable" || name == move + L"_Enable")
        return true;
    for (const auto *direction : {L"Up", L"Down", L"Left", L"Right"})
        if (name == move + direction + L"_Func" || name == move + direction + L"_Enable")
            return true;
    return false;
}
inline bool MultiFingerField(const std::wstring &name) {
    return name == L"MultiFingerGesture_Enable" || FingerGroupField(name, 3) ||
           FingerGroupField(name, 4);
}

// Work only with validated fields supplied by the caller. Missing driver values
// are never invented. Remember choices until this finger count is enabled.
inline void SetFingerGroupEnabled(DriverValues &values, DriverValues &remembered,
                                  int fingers, bool enabled) {
    if (fingers != 3 && fingers != 4)
        return;
    if (enabled) {
        for (auto it = remembered.begin(); it != remembered.end();) {
            if (!FingerGroupField(it->first, fingers)) {
                ++it;
                continue;
            }
            if (values.count(it->first))
                values[it->first] = it->second;
            it = remembered.erase(it);
        }
    } else {
        for (auto &entry : values) {
            if (!FingerGroupField(entry.first, fingers))
                continue;
            remembered.emplace(entry.first, entry.second);
            if (entry.first == L"Tap_Three_Finger" || entry.first == L"Tap_Four_Finger")
                entry.second = DriverNoAction;
            else if (entry.first.size() >= 7 &&
                     entry.first.compare(entry.first.size() - 7, 7, L"_Enable") == 0)
                entry.second = 0;
        }
    }
}
} // namespace app
