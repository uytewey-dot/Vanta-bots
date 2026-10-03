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
    Check(!Cadence.Ready(End + 0.1) && Cadence.Ready(End + AutoFar.Release + 0.02),
        "a real trigger release gap separates automatic bursts");
    Check(!Cadence.Ready(NaN) && !Cadence.Ready(-1.) && Cadence.ShouldRelease(NaN),
        "invalid clock cannot keep firing or start a shot");
    Check(!Cadence.Pressed(3., { NaN, 0.1, 0.2, true }) &&
        !Cadence.Pressed(3., { 0.1, 0., 0.2, true }), "invalid trigger plans cannot start fire");

    for (double Delta : { 1. / 20., 1. / 30., 1. / 60. })
    {
        Combat::AimFollower Follower;
        Combat::Rotation Tracking{}, Baseline{};
        double TrackedError = 0., BaselineError = 0.;
        bool Safe = true;
        for (int Frame = 1; Frame <= int(5. / Delta); ++Frame)
        {
            // Crossing runner observed by a strafing bot, with an abrupt reversal.
            const double Time = Frame * Delta;
            const double TargetY = Time < 2.5 ? Time * 800. : (5. - Time) * 800.;
            const Combat::Vector Origin{ 0., std::sin(Time) * 200., 60. };
            const Combat::Vector Body{ 1800., TargetY, 35. };
            Combat::Rotation DesiredTracking, Next, Plain;
            Safe &= Combat::LookAt(Origin, Body, DesiredTracking) &&
                Follower.Update(Tracking, DesiredTracking, Delta, Next) &&
                Combat::SmoothAim(Baseline, DesiredTracking, Delta, Plain);
            Safe &= std::abs(Combat::NormalizeAngle(Next.Yaw - Tracking.Yaw)) <= 300. * Delta + 1e-8;
            const double Before = Combat::NormalizeAngle(DesiredTracking.Yaw - Tracking.Yaw);
            const double After = Combat::NormalizeAngle(DesiredTracking.Yaw - Next.Yaw);
            Safe &= Before * After >= -1e-8; // No lead beyond current hitscan aim.
            if (Time > 0.6)
            {
                TrackedError += std::abs(After);
                BaselineError += std::abs(Combat::NormalizeAngle(DesiredTracking.Yaw - Plain.Yaw));
            }
            Tracking = Next;
            Baseline = Plain;
        }
        Check(Safe && TrackedError < BaselineError * 0.3,
            "motion feed-forward sharply reduces tracking lag without overshoot across tick rates");
    }
    Combat::AimFollower Follower;
    Check(Follower.Update({ 0., 179. }, { 0., 179. }, 0.05, Result) &&
        Follower.Update(Result, { 0., -179. }, 0.05, Result) && Near(Result.Yaw, -179.),
        "angular tracking crosses yaw wrap without a false large turn");
    Check(!Follower.Update(Result, { NaN, 0. }, 0.05, Result) && !Follower.HasPrevious,
        "invalid tracking input discards angular history");
    Follower.Reset();
    Check(Follower.Update({}, { 0., 90. }, 0.05, Result) && Near(Result.Yaw, 15.),
        "a fresh target cannot inherit previous angular velocity");

    for (Combat::Vector Velocity : { Combat::Vector{ 0., 900., 0. },
        Combat::Vector{ -500., 500., 100. }, Combat::Vector{ 500., -700., -200. } })
    {
        Combat::MotionTracker Intercept;
        Intercept.Sample({ 1000. - Velocity.X * 0.05, -Velocity.Y * 0.05,
            -Velocity.Z * 0.05 }, 1.);
        Intercept.Sample({ 1000., 0., 0. }, 1.05);
        const auto Point = Intercept.AimPoint({}, 10000.);
        const double Time = Combat::Length(Point) / 10000.;
        Check(Near(Point.X, 1000. + Velocity.X * Time, 1e-7) &&
            Near(Point.Y, Velocity.Y * Time, 1e-7) && Near(Point.Z, Velocity.Z * Time, 1e-7),
            "analytic intercept meets the target at actual projectile arrival time");
    }
    Combat::MotionTracker EqualSpeed;
    EqualSpeed.Sample({ 200., 0., 0. }, 1.);
    EqualSpeed.Sample({ 150., 0., 0. }, 1.05);
    Check(Near(EqualSpeed.AimPoint({}, 1000.).X, 75.), "linear intercept handles equal closing speeds");
    EqualSpeed.Sample({ 200., 0., 0. }, 1.10);
    Check(Near(EqualSpeed.AimPoint({}, 1000.).X, 200.), "unreachable receding target has no imaginary intercept");

    const auto Rifle = Combat::Profile(8000., 10000., false, 8.);
    const auto Precision = Combat::Profile(10000., 15000., true, 1.);
    const auto Short = Combat::Profile(8000., 2000., false, 2.);
    Check(Near(Rifle.Range, 8000.) && Near(Short.Range, 1800.) && Short.Preferred < Rifle.Preferred,
        "native reach limits engagement and close-range weapons choose a closer position");
    Check(Precision.Precision && Precision.Reaction > Rifle.Reaction && Short.Reaction < Rifle.Reaction,
        "weapon precision selects acquisition time");
    for (double Invalid : { NaN, Infinity, -1., 0., Huge })
        Check(Near(Combat::Profile(8000., Invalid, false, 8.).Range, 8000.),
            "unknown or unbounded native range preserves the configured cap");
    Check(Combat::Profile(NaN, 10000., false, 8.).Range == 500., "invalid range configuration has bounded fallback");

    Combat::Maneuver Tactics;
    Check(Tactics.Update(1., 7000., Rifle, false, 0).Forward > 0., "advance toward useful weapon range");
    Check(Tactics.Update(1.05, Rifle.Preferred * 1.02, Rifle, false, 0).Forward > 0.,
        "approach remains latched until the preferred distance");
    Check(Tactics.Update(1.10, Rifle.Preferred, Rifle, false, 0).Forward == 0. &&
        Tactics.Update(1.15, Rifle.Preferred * 1.04, Rifle, false, 0).Forward == 0.,
        "small range noise does not cause forward oscillation");
    Check(Tactics.Update(1.20, Rifle.Minimum * 0.9, Rifle, false, 0).Forward < 0. &&
        Tactics.Update(1.25, Rifle.Minimum * 1.1, Rifle, false, 0).Forward < 0. &&
        Tactics.Update(1.30, Rifle.Minimum * 1.3, Rifle, false, 0).Forward == 0.,
        "retreat uses its own hysteresis band");
    Check(std::abs(Tactics.Update(1.35, Rifle.Preferred, Rifle, true, 0).Strafe) > 0.5,
        "reload creates an evasive maneuver");
    Check(Tactics.Update(NaN, 1000., Rifle, false, 0).Strafe == 0. && Tactics.PreviousTime < 0.,
        "invalid clock clears tactical motion");
    Combat::ShotStability Settled;
    Check(!Settled.Observe(true, 1., 0.09) && Settled.Observe(true, 1.10, 0.09),
        "precision shots need sustained alignment");
    Check(!Settled.Observe(false, 1.11, 0.09) && !Settled.Observe(true, 1.12, 0.09),
        "recoil or movement resets the settling window");
    Check(!Settled.Observe(true, 2., 0.09), "stale frames cannot reuse a settled aim");

    Combat::Maneuver BlockedApproach;
    int AdvanceFrames = 0, PlantFrames = 0;
    for (int Tick = 0; Tick < 200; ++Tick)
    {
        const auto Move = BlockedApproach.Update(1. + Tick * 0.05, 7000., Rifle, false, 0);
        AdvanceFrames += Move.Forward > 0.;
        PlantFrames += Move.Forward == 0. && Move.Strafe == 0.;
    }
    Check(AdvanceFrames > 20 && PlantFrames > 50,
        "blocked radial pursuit still schedules shooting windows inside weapon range");

    for (double Delta : { 1. / 20., 1. / 30., 1. / 60. })
    {
        // Integrate native-like acceleration/friction while tactics alternate
        // repositioning and planted precision shots. This catches starvation of
        // the fire window and bursts that continue into movement or occlusion.
        Combat::Maneuver MotionPlan;
        Combat::ShotStability Plant;
        Combat::AimWindow Visibility;
        Combat::AimFollower Tracking;
        Combat::TriggerCadence Trigger;
        Combat::MotionTracker Shooter;
        Combat::Vector Position{};
        Combat::Rotation Facing{};
        double Velocity = 0.;
        bool Held = false, Safe = true;
        int Bursts = 0, MovingFrames = 0, DirectionChanges = 0;
        double LastDirection = 0.;
        for (int Frame = 1; Frame <= int(12. / Delta); ++Frame)
        {
            const double Now = Frame * Delta;
            const auto Move = MotionPlan.Update(Now, Precision.Preferred, Precision, false, 0);
            Velocity += (Move.Strafe * 600. - Velocity) * (1. - std::exp(-10. * Delta));
            Position.Y += Velocity * Delta;
            Shooter.Sample(Position, Now);
            if (Move.Strafe != 0.)
            {
                ++MovingFrames;
                if (LastDirection != 0. && LastDirection * Move.Strafe < 0.)
                    ++DirectionChanges;
                LastDirection = Move.Strafe;
            }
            const bool Visible = !(Now > 5. && Now < 5.6);
            const double AimDelta = Visibility.Observe(Visible, Now);
            Combat::Rotation Look, Next;
            const Combat::Vector Target{ Precision.Preferred, std::sin(Now * 0.6) * 300., 0. };
            const bool Aligned = Combat::LookAt(Position, Target, Look) &&
                Tracking.Update(Facing, Look, AimDelta, Next) &&
                Combat::Aligned(Next, Look, Combat::Length(Combat::Difference(Target, Position)));
            if (AimDelta > 0.)
                Facing = Next;
            const bool Stable = Plant.Observe(Visible && Aligned && Move.Strafe == 0. &&
                Shooter.HasVelocity && std::abs(Shooter.Velocity.Y) <= 110., Now, 0.09);
            const bool FireAllowed = Stable && Visibility.Ready(Now, Precision.Reaction);
            if (Held && (!FireAllowed || Trigger.ShouldRelease(Now)))
            {
                Held = false;
                Trigger.Released(Now);
            }
            if (!Held && FireAllowed && Trigger.Ready(Now))
            {
                Held = Trigger.Pressed(Now, Combat::PlanTrigger(Combat::TriggerMode::Automatic,
                    8., Precision.Preferred, true));
                Bursts += Held;
            }
            Safe &= !Held || (Visible && Aligned && Move.Strafe == 0. && std::abs(Velocity) < 111.);
        }
        Check(Safe && Bursts >= 8 && MovingFrames > 20 && DirectionChanges >= 2 && DirectionChanges <= 4,
            "sustained precision engagement repositions, settles and fires without oscillation or blind bursts");
    }

    if (!Failures)
        std::puts("Player bot combat tests passed.");
    return Failures ? 1 : 0;
}
