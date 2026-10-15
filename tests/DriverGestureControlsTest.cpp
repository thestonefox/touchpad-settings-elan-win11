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

int main() {
    const DriverValues original = {
        {L"MultiFingerGesture_Enable", 1},
        {L"Tap_Three_Finger", 2561}, {L"Tap_Three_Finger_Enable", 1},
        {L"Tap_Four_Finger", 19}, {L"Tap_Four_Finger_Enable", 0},
        {L"ThreeFingerMove_Enable", 1}, {L"FourFingerMove_Enable", 0},
        {L"ThreeFingerMoveUp_Func", 2569}, {L"ThreeFingerMoveUp_Enable", 1},
        {L"FourFingerMoveRight_Func", 2574}, {L"FourFingerMoveRight_Enable", 0},
        {L"Tap_Two_Finger", 1}, {L"Tap_Two_Finger_Enable", 1},
        {L"Zoom_Enable", 1}, {L"SC_Enable", 1}, {L"Unknown_Enable", 1}};
    auto values = original;
    DriverValues remembered;
    SetMultiFingerEnabled(values, remembered, false);
    Check(values.at(L"Tap_Three_Finger") == 7, "Search tap is neutralized, not just disabled");
    Check(values.at(L"Tap_Four_Finger") == 7, "four-finger tap also neutralized");
    Check(values.at(L"Tap_Three_Finger_Enable") == 0 &&
          values.at(L"Tap_Four_Finger_Enable") == 0, "both tap flags disabled");
    Check(values.at(L"MultiFingerGesture_Enable") == 0 &&
          values.at(L"ThreeFingerMove_Enable") == 0 &&
          values.at(L"FourFingerMove_Enable") == 0, "master and swipe switches disabled");
    Check(values.at(L"ThreeFingerMoveUp_Enable") == 0 &&
          values.at(L"FourFingerMoveRight_Enable") == 0, "direction flags disabled");
    Check(values.at(L"ThreeFingerMoveUp_Func") == 2569, "swipe assignment retained");
    for (const auto *name : {L"Tap_Two_Finger", L"Tap_Two_Finger_Enable", L"Zoom_Enable",
                             L"SC_Enable", L"Unknown_Enable"}) {
        Check(values.at(name) == original.at(name), "unrelated setting untouched");
        Check(!remembered.count(name), "unrelated setting not remembered");
    }
    const auto firstRemembered = remembered;
    SetMultiFingerEnabled(values, remembered, false);
    Check(remembered == firstRemembered, "repeated disable preserves original assignments");
    // Both maps are plain values persisted by the application's local settings.
    auto reopenedValues = values;
    auto reopenedRemembered = remembered;
    SetMultiFingerEnabled(reopenedValues, reopenedRemembered, true);
    Check(reopenedValues == original, "reenable restores exact choices, including disabled ones");
    Check(reopenedRemembered.empty(), "successful restoration consumes remembered choices");
    SetMultiFingerEnabled(reopenedValues, reopenedRemembered, true);
    Check(reopenedValues == original, "repeated enable keeps choices");
    DriverValues partial{{L"MultiFingerGesture_Enable", 1}, {L"Tap_Three_Finger", 2561}};
    DriverValues partialRemembered;
    SetMultiFingerEnabled(partial, partialRemembered, false);
    Check(partial.size() == 2 && partial.at(L"Tap_Three_Finger") == 7,
          "missing driver fields are not invented");
    partial.erase(L"Tap_Three_Finger");
    SetMultiFingerEnabled(partial, partialRemembered, true);
    Check(partial.size() == 1, "removed fields are not recreated on restoration");
    DriverValues empty, emptyRemembered;
    SetMultiFingerEnabled(empty, emptyRemembered, false);
    SetMultiFingerEnabled(empty, emptyRemembered, true);
    Check(empty.empty(), "unsupported driver stays untouched");
    Check(!MultiFingerField(L"ThreeFingerMoveAnything_Enable"), "unknown driver field rejected");

    // Exercise the production staging path and on-disk format in memory.
    saved.clear();
    extra.clear();
    appliedSuspended.clear();
    pendingSuspended.clear();
    for (const auto &entry : original)
        saved[entry.first] = Dword(entry.second);
    for (int fingerPage : {1, 2}) {
        page = fingerPage;
        const auto tap = DriverGestures(page == 1 ? 3 : 4)[0];
        SetGestureAction(0, -1);
        Check(DriverNumber(tap.action) == 7 && DriverNumber(tap.enable) == 0,
              "individual tap Off neutralizes its action and enable flag");
        SetGestureAction(0, 2561);
        Check(DriverNumber(tap.action) == 2561 && DriverNumber(tap.enable) == 1,
              "individual tap can be reassigned after Off");
        extra.clear();
    }
    SetMultiFinger(false);
    const auto disabledEdits = extra;
    SetGestureAction(0, 2561);
    Check(extra == disabledEdits, "individual actions cannot bypass disabled master");
    auto changes = Changes();
    Check(changes.at(L"Tap_Three_Finger") == Dword(7), "app stages Search removal");
    Check(changes.at(L"Tap_Four_Finger") == Dword(7), "app stages four-finger action removal");
    Check(Dirty(), "shared disable enables Apply");
    Check(!changes.count(L"Tap_Two_Finger") && !changes.count(L"Zoom_Enable"),
          "app excludes two-finger settings from shared disable");
    SetMultiFinger(true);
    Check(Changes().empty() && !Dirty(), "toggle off/on before Apply cancels all staged edits");
    SetMultiFinger(false);
    // Simulate successful Apply, then a restart, without touching files/registry.
    for (const auto &entry : Changes())
        saved[entry.first] = entry.second;
    extra.clear();
    appliedSuspended = pendingSuspended;
    auto persisted = Decode(Encode(WithAppSettings({})));
    auto restored = DecodeSuspended(persisted);
    Check(restored == pendingSuspended, "remembered choices survive real serialization");
    Check(DecodeFive(persisted, {}) == appliedFive, "five-finger settings preserved");
    Check(DecodeSuspended(Decode(Encode(WithAppSettings(saved)))) == pendingSuspended,
          "typed backups include remembered choices");
    pendingSuspended = restored;
    SetMultiFinger(true);
    auto restoredChanges = Changes();
    for (const auto &entry : original)
        if (MultiFingerField(entry.first)) {
            auto change = restoredChanges.find(entry.first);
            auto actual = change == restoredChanges.end() ? saved.at(entry.first) : change->second;
            Check(actual == Dword(entry.second), "app restores exact choices after restart");
        }
    Check(pendingSuspended.empty() && Dirty(), "restoration stages local-state cleanup");
    Check(DecodeSuspended({}).empty(), "older settings/backups without metadata supported");
    for (const auto *name : {L"@Studio.Suspended.Zoom_Enable",
                             L"@Studio.Suspended.Tap_Three_Finger"}) {
        bool rejected = false;
        try {
            DecodeSuspended({{name, Dword(999999)}});
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected, "invalid or unrelated remembered field rejected");
    }
    std::printf("PASS: %d driver gesture control checks; no registry or input access.\n", checks);
}
