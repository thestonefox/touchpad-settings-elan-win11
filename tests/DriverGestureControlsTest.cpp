// Compile the real app staging and serialization code without entering its GUI.
// These tests never call Apply, Reset, ReadLocal, WriteLocal or the vendor API.
#define wWinMain UnusedTouchpadStudioGuiEntry
#include "../src/TouchpadStudio.cpp"
#undef wWinMain
#include <cstdio>
#include <cstdlib>

using namespace app;
static int checks = 0;
static void Check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static DriverValues Original() {
    DriverValues values{{L"MultiFingerGesture_Enable", 1},
                        {L"Tap_Two_Finger", 1}, {L"Zoom_Enable", 1},
                        {L"SC_Enable", 1}, {L"SC_V_Enable", 1}, {L"SC_H_Enable", 1},
                        {L"SC_Reverse_Enable", 0}, {L"Tap_Two_Finger_Enable", 1},
                        {L"Unknown_Enable", 1}};
    for (int fingers : {3, 4}) {
        auto gestures = DriverGestures(fingers);
        for (int slot = 0; slot < 5; ++slot) {
            values[gestures[slot].action] = slot == 0 ? 2561 : 2569;
            values[gestures[slot].enable] = slot == 4 ? 0 : 1;
        }
        values[fingers == 3 ? L"ThreeFingerMove_Enable" : L"FourFingerMove_Enable"] = 1;
    }
    return values;
}
static void Seed(const DriverValues &values, std::array<bool, 2> enabled = {true, true}) {
    saved.clear();
    for (const auto &entry : values)
        saved[entry.first] = Dword(entry.second);
    extra.clear();
    appliedSuspended.clear();
    pendingSuspended.clear();
    appliedDriverEnabled = pendingDriverEnabled = enabled;
    appliedFive = pendingFive = {true, {FiveDesktop, FiveTaskView, FiveMute, 0, 0}};
    available.fill(false);
    loadedAction = -1;
}
static void SimulateApplyAndRestart() {
    for (const auto &entry : Changes())
        saved[entry.first] = entry.second;
    extra.clear();
    appliedSuspended = pendingSuspended;
    appliedDriverEnabled = pendingDriverEnabled;
    auto local = Decode(Encode(WithAppSettings({})));
    pendingSuspended = DecodeSuspended(local);
    pendingDriverEnabled = DecodeDriverEnabled(local, {true, true});
    Check(pendingSuspended == appliedSuspended, "remembered choices survive serialization");
    Check(pendingDriverEnabled == appliedDriverEnabled, "independent switches survive restart");
    Check(DecodeFive(local, {}) == appliedFive, "five-finger config stays independent");
}
int main() {
    const auto original = Original();
    for (int fingers : {3, 4}) {
        Seed(original);
        page = fingers - 2;
        const auto tap = DriverGestures(fingers)[0];
        SetGestureAction(0, -1);
        Check(DriverNumber(tap.action) == 7 && DriverNumber(tap.enable) == 0,
              "individual tap Off neutralizes the assigned action");
        SetGestureAction(0, 19);
        Check(DriverNumber(tap.action) == 19 && DriverNumber(tap.enable) == 1,
              "individual tap can be reassigned");
        Seed(original);
        SetFingerGroup(fingers, false);
        Check(!FingerGroupEnabled(), "current tab is disabled");
        Check(DriverNumber(tap.action) == 7, "disabled group tap is neutralized");
        Check(DriverNumber(L"MultiFingerGesture_Enable") == 1,
              "shared driver gate stays on for enabled sibling");
        for (const auto &entry : original) {
            if (FingerGroupField(entry.first, fingers) &&
                entry.first.find(L"_Enable") != std::wstring::npos)
                Check(DriverNumber(entry.first) == 0, "all selected group enable fields cleared");
            if (!FingerGroupField(entry.first, fingers))
                Check(DriverNumber(entry.first) == entry.second,
                      "sibling and unrelated driver values unchanged");
        }
        Check(appliedFive == pendingFive, "five-finger switch unchanged");
        const auto disabledEdits = extra;
        SetGestureAction(0, 2561);
        Check(extra == disabledEdits, "individual actions cannot bypass disabled group");
        const auto remembered = pendingSuspended;
        SetFingerGroup(fingers, false);
        Check(pendingSuspended == remembered, "repeated disable retains original choices");
        SetFingerGroup(fingers, true);
        Check(!Dirty(), "off/on before Apply cancels staged changes");
        SetFingerGroup(fingers, false);
        SimulateApplyAndRestart();
        SetFingerGroup(fingers, true);
        for (const auto &entry : original)
            Check(DriverNumber(entry.first) == entry.second, "exact choices restored after restart");
        Check(pendingSuspended.empty(), "restored group clears its remembered choices");
    }
    Seed(original);
    SetFingerGroup(3, false);
    SetFingerGroup(4, false);
    Check(DriverNumber(L"MultiFingerGesture_Enable") == 1,
          "both groups off preserves shared engine for scrolling and pinch");
    for (const auto *name : {L"Zoom_Enable", L"SC_Enable", L"SC_V_Enable",
                             L"SC_H_Enable", L"SC_Reverse_Enable", L"Tap_Two_Finger_Enable"})
        Check(DriverNumber(name) == original.at(name), "two-finger preferences stay unchanged");
    SimulateApplyAndRestart();
    SetFingerGroup(3, true);
    Check(pendingDriverEnabled == std::array<bool, 2>{true, false}, "only three fingers reenabled");
    Check(DriverNumber(L"Tap_Three_Finger") == 2561 && DriverNumber(L"Tap_Four_Finger") == 7,
          "restoring three fingers does not reactivate four-finger tap");
    Check(DriverNumber(L"FourFingerMove_Enable") == 0, "four-finger swipes remain disabled");
    Check(!pendingSuspended.count(L"Tap_Three_Finger") &&
          pendingSuspended.count(L"Tap_Four_Finger"), "sibling restoration state retained");
    auto backup = Decode(Encode(WithAppSettings(saved)));
    Check(DecodeSuspended(backup) == appliedSuspended, "typed backup preserves both groups");
    Check(DecodeDriverEnabled(backup, {true, true}) == appliedDriverEnabled,
          "typed backup preserves independent switches");
    SetFingerGroup(4, true);
    for (const auto &entry : original)
        Check(DriverNumber(entry.first) == entry.second, "both groups restore original configuration");

    // Migrate pre-1.1.2 files with only the shared flag and remembered fields.
    Seed(original);
    auto legacy = saved;
    legacy[L"MultiFingerGesture_Enable"] = Dword(0);
    Check(InitialDriverEnabled(legacy, {}) == std::array<bool, 2>{false, false},
          "old shared-off configuration initializes both switches off");
    DriverValues legacyRemembered = original;
    for (auto it = legacyRemembered.begin(); it != legacyRemembered.end();)
        if (!MultiFingerField(it->first)) it = legacyRemembered.erase(it); else ++it;
    appliedDriverEnabled = pendingDriverEnabled = InitialDriverEnabled(legacy, legacyRemembered);
    saved = legacy;
    appliedSuspended = pendingSuspended = legacyRemembered;
    SetFingerGroup(3, true);
    Check(DriverNumber(L"Tap_Three_Finger") == 2561 && DriverNumber(L"Tap_Four_Finger") == 7,
          "migrated shared-off state can restore only one group");
    Check(!pendingSuspended.count(L"MultiFingerGesture_Enable"), "obsolete shared snapshot removed");
    Seed(original, {false, false});
    saved[L"MultiFingerGesture_Enable"] = Dword(0);
    SetFingerGroup(4, true);
    Check(DriverNumber(L"Tap_Three_Finger") == 7 && DriverNumber(L"Tap_Four_Finger") == 2561,
          "enabling one legacy group neutralizes disabled sibling even without saved metadata");

    Seed(original, {false, false});
    saved[L"MultiFingerGesture_Enable"] = Dword(0);
    StageSharedGateRepair();
    Check(saved.at(L"MultiFingerGesture_Enable") == Dword(0), "repair does not write saved state");
    Check(Changes().at(L"MultiFingerGesture_Enable") == Dword(1), "repair reopens shared engine");
    Check(pendingDriverEnabled == std::array<bool, 2>{false, false}, "repair keeps both counts off");
    Check(DriverNumber(L"Tap_Three_Finger") == 7 && DriverNumber(L"Tap_Four_Finger") == 7,
          "repair does not revive Search or other disabled taps");
    Check(DriverNumber(L"ThreeFingerMove_Enable") == 0 &&
          DriverNumber(L"FourFingerMove_Enable") == 0, "repair keeps swipes disabled");
    for (const auto *name : {L"Zoom_Enable", L"SC_Enable", L"SC_V_Enable", L"SC_H_Enable"})
        Check(DriverNumber(name) == 1, "repair preserves enabled two-finger functions");
    Check(Dirty(), "repair makes Apply available");
    const auto repair = extra;
    StageSharedGateRepair();
    Check(extra == repair, "repair is idempotent");
    SimulateApplyAndRestart();
    Check(!Dirty(), "committed repair does not keep staging changes");
    auto offPreferences = original;
    offPreferences[L"Zoom_Enable"] = offPreferences[L"SC_Enable"] = 0;
    Seed(offPreferences);
    SetFingerGroup(3, false);
    SetFingerGroup(4, false);
    Check(DriverNumber(L"Zoom_Enable") == 0 && DriverNumber(L"SC_Enable") == 0,
          "repair does not turn on two-finger options the user disabled");

    DriverValues partial{{L"Tap_Three_Finger", 2561}}, remembered;
    SetFingerGroupEnabled(partial, remembered, 3, false);
    Check(partial.size() == 1 && partial.begin()->second == 7, "missing fields not invented");
    partial.clear();
    SetFingerGroupEnabled(partial, remembered, 3, true);
    Check(partial.empty(), "removed fields not recreated");
    Check(!MultiFingerField(L"ThreeFingerMoveAnything_Enable"), "unknown driver fields rejected");
    Check(!FingerGroupField(L"Tap_Four_Finger", 3), "group field classification is independent");
    Check(DecodeSuspended({}).empty(), "old files without remembered choices supported");
    for (const Snapshot &bad : {
             Snapshot{{L"@Studio.Suspended.Zoom_Enable", Dword(1)}},
             Snapshot{{L"@Studio.Suspended.Tap_Three_Finger", Dword(999999)}},
             Snapshot{{DriverEnableNames[0], Dword(1)}},
             Snapshot{{DriverEnableNames[0], Dword(2)}, {DriverEnableNames[1], Dword(1)}}}) {
        bool rejected = false;
        try {
            DecodeSuspended(bad);
            DecodeDriverEnabled(bad, {true, true});
        } catch (const std::exception &) { rejected = true; }
        Check(rejected, "invalid or incomplete saved configuration rejected");
    }
    Seed(original);
    appliedFive.swipesEnabled = false;
    pendingFive = appliedFive;
    auto fiveBackup = Decode(Encode(WithAppSettings({})));
    Check(DecodeFive(fiveBackup, {}).swipesEnabled == false,
          "five-finger swipe switch survives settings and backup serialization");
    Check(DecodeFive({}, appliedFive).swipesEnabled,
          "older five-finger settings retain the original swipe behaviour");
    bool rejectedSwipe = false;
    try { DecodeFive({{L"@Studio.FiveSwipesEnabled", Dword(2)}}, {}); }
    catch (const std::exception &) { rejectedSwipe = true; }
    Check(rejectedSwipe, "invalid five-finger swipe setting rejected");
    page = 3;
    Check(PageRows() == 7, "five-finger layout includes master, swipes and five actions");
    for (int i = 0; i < 2; ++i) {
        pendingDriverEnabled[i] = false;
        SetFingerGroup(i + 3, false);
    }
    Check(!pendingFive.swipesEnabled && pendingFive.enabled,
          "three/four controls preserve five-finger tap-only mode");
    std::printf("PASS: %d driver gesture control checks; no registry or input access.\n", checks);
}
