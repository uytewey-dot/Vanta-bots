#pragma once

#include <algorithm>
#include <cmath>

// Engine-independent decisions for server-owned bots. All distances are Unreal
// centimetres, all angles are degrees, and times are monotonic game seconds.
namespace PlayerBotCombat
{
    constexpr double Degrees = 57.29577951308232;

    struct Vector
    {
        double X = 0., Y = 0., Z = 0.;
    };

    struct Rotation
    {
        double Pitch = 0., Yaw = 0.;
    };

    inline bool Finite(const Vector& Value) noexcept
    {
        return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
    }

    inline double Length(const Vector& Value) noexcept
    {
        return std::hypot(std::hypot(Value.X, Value.Y), Value.Z);
    }

    inline Vector Difference(const Vector& A, const Vector& B) noexcept
    {
        return { A.X - B.X, A.Y - B.Y, A.Z - B.Z };
    }

    inline double NormalizeAngle(double Angle) noexcept
    {
        return std::remainder(Angle, 360.);
    }

    inline bool LookAt(const Vector& From, const Vector& To, Rotation& Result) noexcept
    {
        if (!Finite(From) || !Finite(To))
            return false;
        const auto Delta = Difference(To, From);
        const double Distance = Length(Delta);
        if (!std::isfinite(Distance) || Distance < 1.)
            return false;
        Result = { std::atan2(Delta.Z, std::hypot(Delta.X, Delta.Y)) * Degrees,
            std::atan2(Delta.Y, Delta.X) * Degrees };
        return true;
    }

    inline bool SmoothAim(const Rotation& Current, const Rotation& Desired,
        double DeltaSeconds, Rotation& Result) noexcept
    {
        if (!std::isfinite(Current.Pitch) || !std::isfinite(Current.Yaw) ||
            !std::isfinite(Desired.Pitch) || !std::isfinite(Desired.Yaw) ||
            !std::isfinite(DeltaSeconds) || DeltaSeconds <= 0. || DeltaSeconds > 0.25)
            return false;
        // A hitch must not let the bot instantly turn around. Exponential easing
        // approaches the target smoothly while a hard rate cap bounds large turns.
        const double Step = (std::min)(DeltaSeconds, 0.1);
        const double Blend = -std::expm1(-22. * Step);
        const double Yaw = NormalizeAngle(Current.Yaw);
        const double YawDelta = NormalizeAngle(NormalizeAngle(Desired.Yaw) - Yaw);
        const double Pitch = (std::clamp)(NormalizeAngle(Current.Pitch), -89., 89.);
        const double PitchDelta = (std::clamp)(NormalizeAngle(Desired.Pitch), -89., 89.) - Pitch;
        Result = { (std::clamp)(Pitch + (std::clamp)(PitchDelta * Blend,
                -180. * Step, 180. * Step), -89., 89.),
            NormalizeAngle(Yaw + (std::clamp)(YawDelta * Blend,
                -300. * Step, 300. * Step)) };
        return true;
    }

    inline bool Aligned(const Rotation& Current, const Rotation& Desired,
        double Distance) noexcept
    {
        if (!std::isfinite(Current.Pitch) || !std::isfinite(Current.Yaw) ||
            !std::isfinite(Desired.Pitch) || !std::isfinite(Desired.Yaw) ||
            !std::isfinite(Distance) || Distance < 1.)
            return false;
        // Aim within a torso-sized radius; the native weapon still applies spread.
        const double Tolerance = (std::clamp)(std::atan2(25., Distance) * Degrees, 0.15, 2.);
        return std::hypot(NormalizeAngle(NormalizeAngle(Desired.Yaw) - NormalizeAngle(Current.Yaw)),
            NormalizeAngle(NormalizeAngle(Desired.Pitch) - NormalizeAngle(Current.Pitch))) <= Tolerance;
    }

    struct MotionTracker
    {
        Vector Position{};
        Vector Velocity{};
        double Time = -1.;
        bool HasVelocity = false;

        void Reset() noexcept { *this = {}; }

        void Sample(const Vector& NewPosition, double Now) noexcept
        {
            if (!Finite(NewPosition) || !std::isfinite(Now) || Now < 0.)
            {
                Reset();
                return;
            }
            const double Delta = Now - Time;
            const auto Displacement = Difference(NewPosition, Position);
            const double Speed = Length(Displacement) / (Delta > 0. ? Delta : 1.);
            HasVelocity = Time >= 0. && Delta >= 0.01 && Delta <= 0.25 &&
                std::isfinite(Speed) && Speed <= 5000.;
            Velocity = HasVelocity ? Vector{ Displacement.X / Delta,
                Displacement.Y / Delta, Displacement.Z / Delta } : Vector{};
            Position = NewPosition;
            Time = Now;
        }

        Vector AimPoint(const Vector& Origin, double ProjectileSpeed) const noexcept
        {
            // Zero speed is the intentional hitscan/unknown-weapon fallback.
            if (!HasVelocity || !Finite(Origin) || !Finite(Position) || !Finite(Velocity) ||
                !std::isfinite(ProjectileSpeed) || ProjectileSpeed < 1000. ||
                ProjectileSpeed > 200000.)
                return Position;
            const auto Relative = Difference(Position, Origin);
            double FlightTime = Length(Relative) / ProjectileSpeed;
            if (!std::isfinite(FlightTime) || FlightTime <= 0. || FlightTime > 0.35)
                return Position;
            // Short-horizon fixed-point intercept; no invented gravity or ballistics.
            for (int Index = 0; Index < 3; ++Index)
            {
                const Vector Predicted{ Relative.X + Velocity.X * FlightTime,
                    Relative.Y + Velocity.Y * FlightTime, Relative.Z + Velocity.Z * FlightTime };
                FlightTime = Length(Predicted) / ProjectileSpeed;
                if (!std::isfinite(FlightTime) || FlightTime > 0.35)
                    return Position;
            }
            if (Length(Velocity) * FlightTime > 350.)
                return Position;
            return { Position.X + Velocity.X * FlightTime,
                Position.Y + Velocity.Y * FlightTime, Position.Z + Velocity.Z * FlightTime };
        }
    };

    struct AimWindow
    {
        double PreviousTime = -1.;
        double VisibleSince = -1.;

        void Reset() noexcept { *this = {}; }

        double Observe(bool Visible, double Now) noexcept
        {
            if (!Visible || !std::isfinite(Now) || Now < 0.)
            {
                Reset();
                return 0.;
            }
            const double Delta = Now - PreviousTime;
            if (PreviousTime < 0. || Delta <= 0. || Delta > 0.25)
            {
                VisibleSince = Now;
                PreviousTime = Now;
                return 0.;
            }
            PreviousTime = Now;
            return Delta;
        }

        bool Ready(double Now) const noexcept
        {
            return std::isfinite(Now) && VisibleSince >= 0. &&
                Now >= PreviousTime && Now - PreviousTime <= 0.1 && Now - VisibleSince >= 0.18;
        }
    };

    enum class TriggerMode { Unknown, OnPress, Automatic, Unsupported };

    struct TriggerPlan
    {
        double Hold = 0.16;
        double Release = 0.12;
        double Interval = 0.3;
        bool Supported = true;
    };

    inline TriggerPlan PlanTrigger(TriggerMode Mode, double FiringRate, double Distance) noexcept
    {
        if (!std::isfinite(Distance) || Distance < 0. || Mode == TriggerMode::Unsupported)
            return { 0., 0., 0., false };
        const double Interval = std::isfinite(FiringRate) && FiringRate >= 0.2 && FiringRate <= 30.
            ? 1. / FiringRate : 0.3;
        if (Mode == TriggerMode::OnPress)
            return { 0.045, 0.065, (std::max)(Interval, 0.1), true };
        if (Mode == TriggerMode::Automatic)
            return { (std::clamp)(0.50 - Distance / 30000., 0.22, 0.5),
                Distance > 8000. ? 0.2 : 0.12, Interval, true };
        return { 0.16, 0.12, (std::max)(Interval, 0.3), true };
    }

    struct TriggerCadence
    {
        double HoldUntil = 0.;
        double NextPress = 0.;
        double ReleaseGap = 0.12;

        bool Ready(double Now) const noexcept
        {
            return std::isfinite(Now) && Now >= 0. && Now >= NextPress;
        }

        // Called only after successful native start/stop; a failed stop remains
        // held in the runtime and may never issue another start or a reload.
        bool Pressed(double Now, const TriggerPlan& Plan) noexcept
        {
            if (!Ready(Now) || !Plan.Supported || !std::isfinite(Plan.Hold) || Plan.Hold <= 0. ||
                !std::isfinite(Plan.Release) || Plan.Release <= 0. ||
                !std::isfinite(Plan.Interval) || Plan.Interval < 0.)
                return false;
            HoldUntil = Now + Plan.Hold;
            NextPress = Now + Plan.Interval;
            ReleaseGap = Plan.Release;
            return true;
        }

        bool ShouldRelease(double Now) const noexcept
        {
            return !std::isfinite(Now) || Now >= HoldUntil;
        }

        void Released(double Now) noexcept
        {
            if (std::isfinite(Now) && Now >= 0.)
                NextPress = (std::max)(NextPress, Now + ReleaseGap);
        }
    };
}
