#include "../Erbium/Support/Public/PlayerBotCombat.h"

#include <array>
#include <cstdio>
#include <limits>

namespace Combat = PlayerBotCombat;

int main()
{
    int Failures = 0;
    auto Check = [&Failures](bool Result, const char* Name)
    {
        if (!Result)
        {
            std::fprintf(stderr, "FAILED: %s\n", Name);
            ++Failures;
        }
    };
    auto Near = [](double A, double B, double Tolerance = 1e-8)
    {
        return std::isfinite(A) && std::isfinite(B) && std::abs(A - B) <= Tolerance;
    };
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    const double Huge = std::numeric_limits<double>::max();

    Combat::Rotation Desired, Result;
    Check(Combat::LookAt({}, { 1000., 0., 1000. }, Desired) &&
        Near(Desired.Pitch, 45.) && Near(Desired.Yaw, 0.), "look at an elevated body");
    Check(!Combat::LookAt({}, {}, Desired), "no direction at zero distance");
    Check(!Combat::LookAt({ Huge, 0., 0. }, { -Huge, 0., 0. }, Desired),
        "reject overflowing position difference");
    for (double Bad : { NaN, Infinity, -Infinity })
    {
        Check(!Combat::LookAt({}, { Bad, 0., 0. }, Desired), "reject invalid target position");
        Check(!Combat::SmoothAim({}, { 0., Bad }, 0.05, Result), "reject invalid desired angle");
        Check(!Combat::SmoothAim({ Bad, 0. }, {}, 0.05, Result), "reject invalid current angle");
        Check(!Combat::Aligned({}, {}, Bad), "invalid distance cannot pass alignment");
    }
    Check(Combat::SmoothAim({ 0., 179. }, { 0., -179. }, 0.05, Result) &&
        std::abs(Combat::NormalizeAngle(Result.Yaw - 179.)) < 2. && Result.Yaw < -179.,
        "yaw crosses the wrap along the short path");
    Check(Combat::SmoothAim({ 0., -179. }, { 0., 179. }, 0.05, Result) &&
        Result.Yaw > 179., "yaw wrap works in the other direction");
    Check(Combat::SmoothAim({ Huge, Huge }, { -Huge, -Huge }, 0.05, Result) &&
        std::isfinite(Result.Pitch) && std::isfinite(Result.Yaw),
        "normalize huge finite angles before subtracting");
    Check(Combat::SmoothAim({}, { 0., 180. }, 0.05, Result) && Near(std::abs(Result.Yaw), 15.),
        "large turn obeys the angular velocity limit");
    Check(Combat::SmoothAim({}, { 0., 180. }, 0.24, Result) && Near(std::abs(Result.Yaw), 30.),
        "a hitch cannot grant an instant turn");
    for (double BadDelta : { 0., -0.1, 0.251, NaN, Infinity })
        Check(!Combat::SmoothAim({}, { 0., 90. }, BadDelta, Result),
            "invalid or stale delta cannot update aim");
    Check(Combat::Aligned({ 0., 179.99 }, { 0., -179.99 }, 10000.),
        "alignment recognizes wrapped equivalent directions");
    Check(!Combat::Aligned({}, { 0., 1. }, 10000.) &&
        Combat::Aligned({}, { 0., 1. }, 500.), "long-distance aim requires tighter alignment");
    for (double Delta : { 1. / 30., 1. / 60., 1. / 120. })
    {
        Combat::Rotation Current{};
        bool Good = true;
        for (int Tick = 0; Tick < static_cast<int>(1.5 / Delta); ++Tick)
        {
            Combat::Rotation Next;
            Good &= Combat::SmoothAim(Current, { 20., 120. }, Delta, Next);
            Good &= std::abs(Combat::NormalizeAngle(Next.Yaw - Current.Yaw)) <= 300. * Delta + 1e-8;
            Good &= std::abs(Next.Pitch - Current.Pitch) <= 180. * Delta + 1e-8;
            Current = Next;
        }
        Check(Good && Combat::Aligned(Current, { 20., 120. }, 10000.),
            "bounded turn converges across server tick rates");
    }
    for (double Range : { 1000., 5000. })
    {
        for (double Speed : { 600., 800. })
        {
            Combat::Rotation Current{};
            bool TracksRunner = true;
            for (int Tick = 1; Tick <= 100; ++Tick)
            {
                const Combat::Vector Position{ Range, Speed * Tick * 0.05, 0. };
                Combat::Rotation Look, Next;
                TracksRunner &= Combat::LookAt({}, Position, Look) &&
                    Combat::SmoothAim(Current, Look, 0.05, Next);
                if (Tick > 15)
                    TracksRunner &= Combat::Aligned(Next, Look, Combat::Length(Position));
                Current = Next;
            }
            Check(TracksRunner, "20 Hz hitscan tracking can align with a running target without lead");
        }
    }

    Combat::MotionTracker Motion;
    Motion.Sample({ 1000., 0., 0. }, 1.);
    Check(!Motion.HasVelocity, "first observation has no fabricated velocity");
    Motion.Sample({ 1000., 50., 0. }, 1.1);
    Check(Motion.HasVelocity && Near(Motion.Velocity.Y, 500.), "measure velocity from consecutive samples");
    Check(Near(Motion.AimPoint({}, 0.).Y, 50.), "hitscan gets no movement lead");
    const auto Led = Motion.AimPoint({}, 10000.);
    Check(Led.Y > 99. && Led.Y < 102. && Near(Led.X, 1000.), "lead a confirmed fast projectile");
    for (double Speed : { NaN, Infinity, -1., 999., 200001. })
        Check(Near(Motion.AimPoint({}, Speed).Y, 50.), "invalid projectile speed falls back to direct aim");
    Check(Near(Motion.AimPoint({}, 1000.).Y, 50.), "reject long-flight ballistic extrapolation");
    Motion.Sample({ 1000., 0., 0. }, 1.2);
    Check(Motion.HasVelocity && Motion.Velocity.Y < 0. && Motion.AimPoint({}, 10000.).Y < 0.,
        "a direction reversal does not retain old velocity");
    Motion.Sample({ 50000., 0., 0. }, 1.3);
    Check(!Motion.HasVelocity && Near(Motion.AimPoint({}, 10000.).X, 50000.),
        "a teleport cannot become a velocity prediction");
    Motion.Sample({ 50010., 0., 0. }, 2.);
    Check(!Motion.HasVelocity, "a stale sample discards velocity");
    Motion.Sample({ 50020., 0., 0. }, 1.);
    Check(!Motion.HasVelocity, "a reversed clock discards velocity");
    Motion.Sample({ NaN, 0., 0. }, 1.1);
    Check(!Motion.HasVelocity && Motion.Time < 0., "invalid position clears tracking");
    Motion.Sample({ 3000., 0., 0. }, 3.);
    Motion.Sample({ 3000., 400., 0. }, 3.1);
    Check(Motion.HasVelocity && Near(Motion.AimPoint({}, 15000.).Y, 400.),
        "prediction displacement has a physical upper bound");
    Motion.Reset();
    Motion.Sample({ 100., 100., 0. }, 3.2);
    Check(!Motion.HasVelocity, "switching targets starts a fresh motion track");

    Combat::AimWindow Window;
    Check(Near(Window.Observe(true, 1.), 0.) && !Window.Ready(1.), "new targets require reaction time");
    Window.Observe(true, 1.05);
    Window.Observe(true, 1.10);
    Window.Observe(true, 1.15);
    Check(!Window.Ready(1.15), "no early shot during acquisition");
    Window.Observe(true, 1.20);
    Check(Window.Ready(1.20), "stable visibility allows an aligned shot");
    Window.Observe(false, 1.21);
    Check(!Window.Ready(1.21), "lost LOS immediately closes firing gate");
    Window.Observe(true, 1.22);
    Check(!Window.Ready(1.22), "reappearing targets require a fresh reaction window");
    Window.Observe(true, 2.);
    Check(!Window.Ready(2.), "stale frames restart acquisition");
    Window.Observe(true, 1.);
    Check(!Window.Ready(1.), "reversed clock restarts acquisition");
    Window.Observe(true, NaN);
    Check(!Window.Ready(2.), "invalid time clears firing gate");
    Window.Reset();
    Check(!Window.Ready(10.), "target switching cannot reuse reaction time");

    const auto Semi = Combat::PlanTrigger(Combat::TriggerMode::OnPress, 2., 5000.);
    const auto AutoClose = Combat::PlanTrigger(Combat::TriggerMode::Automatic, 10., 1000.);
    const auto AutoFar = Combat::PlanTrigger(Combat::TriggerMode::Automatic, 10., 15000.);
    Check(Semi.Hold < 0.05 && Near(Semi.Interval, 0.5), "semiautomatic cadence respects native firing rate");
    Check(AutoFar.Hold < AutoClose.Hold && AutoFar.Release > AutoClose.Release,
        "long-range automatic fire uses shorter bursts and longer gaps");
    Check(!Combat::PlanTrigger(Combat::TriggerMode::Unsupported, 10., 5000.).Supported,
        "unsupported charge/release weapons do not fire");
    Check(!Combat::PlanTrigger(Combat::TriggerMode::Automatic, 10., NaN).Supported,
        "invalid target range disables fire");
    Check(Near(Combat::PlanTrigger(Combat::TriggerMode::OnPress, NaN, 5000.).Interval, 0.3),
        "unavailable firing rate has a conservative fallback");

    Combat::TriggerCadence Cadence;
    Check(Cadence.Ready(1.) && Cadence.Pressed(1., Semi), "record a successful native trigger press");
    Check(!Cadence.ShouldRelease(1.03) && Cadence.ShouldRelease(1.05),
        "release a semiautomatic trigger promptly after the shot");
    Cadence.Released(1.05);
    Check(!Cadence.Ready(1.49) && Cadence.Ready(1.5), "never exceed the native shot interval");
    Check(!Cadence.Pressed(1.1, Semi), "reject a press during cooldown");
    Check(Cadence.Pressed(1.5, AutoFar), "automatic fire starts after cooldown");
    const double End = 1.5 + AutoFar.Hold;
    Check(!Cadence.ShouldRelease(End - 0.01) && Cadence.ShouldRelease(End + 0.01),
        "automatic fire is held only for the selected burst");
    Cadence.Released(End + 0.01);
    Check(!Cadence.Ready(End + 0.1) && Cadence.Ready(End + 0.22),
        "a real trigger release gap separates automatic bursts");
    Check(!Cadence.Ready(NaN) && !Cadence.Ready(-1.) && Cadence.ShouldRelease(NaN),
        "invalid clock cannot keep firing or start a shot");
    Check(!Cadence.Pressed(3., { NaN, 0.1, 0.2, true }) &&
        !Cadence.Pressed(3., { 0.1, 0., 0.2, true }), "invalid trigger plans cannot start fire");

    if (!Failures)
        std::puts("Player bot combat tests passed.");
    return Failures ? 1 : 0;
}
