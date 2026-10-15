#include "../src/FiveFingerRecognizer.h"
#include <cstdio>
#include <cstdlib>

using namespace app;
static int checks = 0;
static void Check(bool value, const char *name) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", name);
        std::exit(1);
    }
}
static std::array<Contact, 5> Points(float dx = 0, float dy = 0) {
    std::array<Contact, 5> p{};
    for (int i = 0; i < 5; ++i)
        p[i] = {true, .2f + i * .1f + dx, .5f + dy};
    return p;
}
int main() {
    const std::array<Contact, 5> lifted{};
    const FiveConfig cfg{
        true, {FiveDesktop, FiveTaskView, FiveMute, FivePreviousDesktop, FiveNextDesktop}};
    FiveFingerRecognizer r;
    Check(r.Feed(Points(), 1000, cfg) == FiveOff, "contact down never dispatches");
    Check(r.Feed(lifted, 1200, cfg) == FiveDesktop, "stable five-finger tap");
    Check(r.Feed(lifted, 1250, cfg) == FiveOff, "tap fires only once");
    const float moves[4][2] = {{0, -.2f}, {0, .2f}, {-.2f, 0}, {.2f, 0}};
    for (int d = 0; d < 4; ++d) {
        r.Reset();
        r.Feed(Points(), 1000, cfg);
        Check(r.Feed(Points(moves[d][0], moves[d][1]), 1200, cfg) == FiveOff,
              "swipe waits until lift");
        Check(r.Feed(lifted, 1300, cfg) == cfg.actions[d + 1], "swipe direction");
        Check(r.Feed(lifted, 1400, cfg) == FiveOff, "swipe fires only once");
    }
    r.Feed(Points(), 2000, cfg);
    Check(r.Feed(lifted, 2099, cfg) == FiveOff, "brief noise rejected");
    r.Feed(Points(), 2000, cfg);
    Check(r.Feed(lifted, 2551, cfg) == FiveOff, "long tap rejected");
    r.Feed(Points(), 2000, cfg);
    r.Feed(Points(), 3700, cfg);
    Check(r.Feed(lifted, 3800, cfg) == FiveOff, "long hold rejected");
    r.Feed(Points(), 4000, cfg);
    r.Feed(Points(.2f, .2f), 4200, cfg);
    Check(r.Feed(lifted, 4300, cfg) == FiveOff, "ambiguous diagonal rejected");
    auto four = Points();
    four[4].down = false;
    r.Feed(four, 5000, cfg);
    Check(r.Feed(lifted, 5200, cfg) == FiveOff, "four contacts rejected");
    r.Feed(Points(), 6000, cfg);
    r.Feed(four, 6150, cfg);
    r.Feed(Points(), 6200, cfg);
    Check(r.Feed(lifted, 6300, cfg) == FiveOff, "retouch rejected");
    r.Feed(Points(), 7000, cfg);
    r.Feed(four, 7150, cfg);
    r.Feed(four, 7380, cfg);
    Check(r.Feed(lifted, 7400, cfg) == FiveOff, "prolonged partial lift rejected");
    auto spread = Points();
    for (int i = 0; i < 5; ++i)
        spread[i].x += (i - 2) * .15f;
    r.Feed(Points(), 8000, cfg);
    r.Feed(spread, 8200, cfg);
    Check(r.Feed(lifted, 8300, cfg) == FiveOff, "spread rejected");
    auto mixed = Points(.2f);
    mixed[0].x = .1f;
    r.Feed(Points(), 9000, cfg);
    r.Feed(mixed, 9200, cfg);
    Check(r.Feed(lifted, 9300, cfg) == FiveOff, "opposing contact rejected");
    FiveConfig disabled = cfg;
    disabled.enabled = false;
    r.Feed(Points(), 10000, disabled);
    Check(r.Feed(lifted, 10200, disabled) == FiveOff, "disabled configuration");
    r.Feed(Points(), 11000, cfg);
    r.Feed(Points(), 11100, disabled);
    Check(r.Feed(lifted, 11200, cfg) == FiveOff, "disable resets in-flight gesture");
    FiveConfig noAction{true, {}};
    r.Feed(Points(), 12000, noAction);
    Check(r.Feed(lifted, 12200, noAction) == FiveOff, "unassigned tap");
    r.Feed(Points(), 13000, cfg);
    r.Reset();
    Check(r.Feed(lifted, 13200, cfg) == FiveOff, "explicit reset");
    r.Feed(Points(), 14000, cfg);
    Check(r.Feed(lifted, 14200, cfg) == FiveDesktop, "recognition recovers after rejection");
    auto tapOnly = cfg;
    tapOnly.swipesEnabled = false;
    Check(tapOnly != cfg, "swipe switch participates in configuration changes");
    r.Reset();
    r.Feed(Points(), 15000, tapOnly);
    Check(r.Feed(lifted, 15200, tapOnly) == FiveDesktop, "swipes off preserves five-finger tap");
    for (int d = 0; d < 4; ++d) {
        r.Reset();
        r.Feed(Points(), 16000, tapOnly);
        r.Feed(Points(moves[d][0], moves[d][1]), 16200, tapOnly);
        Check(r.Feed(lifted, 16300, tapOnly) == FiveOff, "disabled swipe never becomes a tap");
        r.Reset();
        r.Feed(Points(), 17000, cfg);
        r.Feed(Points(moves[d][0], moves[d][1]), 17200, cfg);
        Check(r.Feed(lifted, 17300, cfg) == cfg.actions[d + 1],
              "reenabling swipes restores assigned actions");
    }
    Check(tapOnly.actions == cfg.actions, "swipe disable retains all assignments");
    std::printf("PASS: %d recognizer checks; no Windows input or registry access.\n", checks);
}
