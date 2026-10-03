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
        double DeltaSeconds, Rotation& Result, const Rotation& TargetStep = {}) noexcept
    {
        if (!std::isfinite(Current.Pitch) || !std::isfinite(Current.Yaw) ||
            !std::isfinite(Desired.Pitch) || !std::isfinite(Desired.Yaw) ||
            !std::isfinite(TargetStep.Pitch) || !std::isfinite(TargetStep.Yaw) ||
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
        // Feed forward observed angular motion to remove steady-state tracking
        // lag. Never extrapolate hitscan aim beyond the current target direction.
        const auto Follow = [Blend](double Error, double Motion)
        {
            return (std::clamp)(Error * Blend + Motion * (1. - Blend),
                (std::min)(0., Error), (std::max)(0., Error));
        };
        Result = { (std::clamp)(Pitch + (std::clamp)(Follow(PitchDelta,
                (std::clamp)(TargetStep.Pitch, -90. * Step, 90. * Step)),
                -180. * Step, 180. * Step), -89., 89.),
            NormalizeAngle(Yaw + (std::clamp)(Follow(YawDelta,
                (std::clamp)(TargetStep.Yaw, -180. * Step, 180. * Step)),
                -300. * Step, 300. * Step)) };
        return true;
    }

    struct AimFollower
    {
        Rotation Previous{};
        bool HasPrevious = false;

        void Reset() noexcept { *this = {}; }

        bool Update(const Rotation& Current, const Rotation& Desired, double Delta,
            Rotation& Result) noexcept
        {
            Rotation Motion{};
            if (HasPrevious && std::isfinite(Delta) && Delta > 0. && Delta <= 0.1)
                Motion = { NormalizeAngle(Desired.Pitch - Previous.Pitch),
                    NormalizeAngle(Desired.Yaw - Previous.Yaw) };
            if (!SmoothAim(Current, Desired, Delta, Result, Motion))
            {
                Reset();
                return false;
            }
            Previous = { NormalizeAngle(Desired.Pitch), NormalizeAngle(Desired.Yaw) };
            HasPrevious = true;
            return true;
        }
    };

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
            // Solve |relative + velocity*t| = projectileSpeed*t exactly. The
            // stable quadratic form also handles nearly equal closing speeds.
            const double A = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y +
                Velocity.Z * Velocity.Z - ProjectileSpeed * ProjectileSpeed;
            const double B = 2. * (Relative.X * Velocity.X + Relative.Y * Velocity.Y +
                Relative.Z * Velocity.Z);
            const double C = Relative.X * Relative.X + Relative.Y * Relative.Y + Relative.Z * Relative.Z;
            if (!std::isfinite(A) || !std::isfinite(B) || !std::isfinite(C))
                return Position;
            double FlightTime = -1.;
            if (std::abs(A) <= ProjectileSpeed * ProjectileSpeed * 1e-10)
            {
                if (B < 0.)
                    FlightTime = -C / B;
            }
            else
            {
                const double Discriminant = B * B - 4. * A * C;
                if (!std::isfinite(Discriminant) || Discriminant < 0.)
                    return Position;
                const double Q = -0.5 * (B + std::copysign(std::sqrt(Discriminant), B));
                const double T1 = Q / A;
                const double T2 = Q != 0. ? C / Q : -1.;
                if (std::isfinite(T1) && T1 > 0.)
                    FlightTime = T1;
                if (std::isfinite(T2) && T2 > 0. && (FlightTime <= 0. || T2 < FlightTime))
                    FlightTime = T2;
            }
            if (!std::isfinite(FlightTime) || FlightTime <= 0. || FlightTime > 0.35 ||
                Length(Velocity) * FlightTime > 350.)
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

        bool Ready(double Now, double Reaction = 0.18) const noexcept
        {
            return std::isfinite(Now) && std::isfinite(Reaction) && Reaction >= 0.1 &&
                Reaction <= 1. && VisibleSince >= 0. && Now >= PreviousTime &&
                Now - PreviousTime <= 0.1 && Now - VisibleSince >= Reaction;
        }
    };

    struct WeaponProfile
    {
        double Range = 500.;
        double Preferred = 300.;
        double Minimum = 150.;
        double Reaction = 0.16;
        bool Precision = false;
    };

    inline WeaponProfile Profile(double ConfiguredRange, double NativeRange,
        bool Scoped, double FiringRate) noexcept
    {
        const double Cap = std::isfinite(ConfiguredRange)
            ? (std::clamp)(ConfiguredRange, 150., 30000.) : 500.;
        const double Range = std::isfinite(NativeRange) && NativeRange >= 150. && NativeRange <= 300000.
            ? (std::min)(Cap, NativeRange * 0.9) : Cap;
        const bool Precision = Scoped || (std::isfinite(FiringRate) &&
            FiringRate >= 0.2 && FiringRate <= 1.5 && Range > 4000.);
        return { Range, Range * (Precision ? 0.75 : 0.6),
            (std::min)(Range * 0.2, Precision ? 1800. : 900.),
            Precision ? 0.22 : (Range <= 2500. ? 0.12 : 0.16), Precision };
    }

    struct ManeuverPlan
    {
        // Signed radial and lateral movement relative to the target on the XY
        // plane. The engine still owns acceleration, collision, and friction.
        double Forward = 0.;
        double Strafe = 0.;
        bool NeedsPlant = false;
    };

    struct Maneuver
    {
        double Started = -1.;
        double PreviousTime = -1.;
        int Radial = 0;

        void Reset() noexcept { *this = {}; }

        ManeuverPlan Update(double Now, double Distance, const WeaponProfile& Weapon,
            bool Reloading, unsigned Variation) noexcept
        {
            if (!std::isfinite(Now) || Now < 0. || !std::isfinite(Distance) || Distance < 0. ||
                !std::isfinite(Weapon.Range) || !std::isfinite(Weapon.Preferred) ||
                !std::isfinite(Weapon.Minimum) || Weapon.Minimum < 0. ||
                Weapon.Minimum >= Weapon.Preferred || Weapon.Preferred >= Weapon.Range)
            {
                Reset();
                return {};
            }
            if (PreviousTime < 0. || Now < PreviousTime || Now - PreviousTime > 0.5)
            {
                Started = Now;
                Radial = 0;
            }
            PreviousTime = Now;
            // Hysteresis avoids running back and forth at the preferred radius.
            if (Distance > Weapon.Preferred * 1.16)
                Radial = 1;
            else if (Distance < Weapon.Minimum)
                Radial = -1;
            else if ((Radial > 0 && Distance <= Weapon.Preferred) ||
                (Radial < 0 && Distance >= Weapon.Minimum * 1.25))
                Radial = 0;
            const double Age = Now - Started + double(Variation % 13) * 0.13;
            const double Phase = std::fmod(Age, 3.2);
            const double Side = std::fmod(Age, 6.4) < 3.2 ? 1. : -1.;
            const bool NeedsPlant = Weapon.Precision || Distance > 3500.;
            if (Radial != 0)
            {
                // These decisions run only inside weapon range. Periodically
                // plant even while closing: collision with a fence must not
                // leave a visible target permanently immune to a ranged bot.
                if (NeedsPlant && !Reloading && Phase >= 1.2)
                    return { 0., 0., true };
                return { Radial > 0 ? 0.8 : -0.65, Side * 0.2, NeedsPlant };
            }
            if (Reloading)
                return { Distance < Weapon.Preferred * 0.8 ? -0.35 : 0., Side * 0.65, NeedsPlant };
            // Long-range shots get a quiet window for native spread to recover;
            // nearby engagements keep a moderate strafe instead of a static bot.
            return { 0., NeedsPlant ? (Phase < 0.8 ? Side * 0.5 : 0.) : Side * 0.4, NeedsPlant };
        }
    };

    struct ShotStability
    {
        double PreviousTime = -1.;
        double Since = -1.;

        void Reset() noexcept { *this = {}; }

        bool Observe(bool AlignedAndSettled, double Now, double Required) noexcept
        {
            if (!AlignedAndSettled || !std::isfinite(Now) || Now < 0. ||
                !std::isfinite(Required) || Required < 0. || Required > 0.5)
            {
                Reset();
                return false;
            }
            if (PreviousTime < 0. || Now <= PreviousTime || Now - PreviousTime > 0.25)
                Since = Now;
            PreviousTime = Now;
            return Now - Since >= Required;
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

    inline TriggerPlan PlanTrigger(TriggerMode Mode, double FiringRate, double Distance,
        bool Precision = false) noexcept
    {
        if (!std::isfinite(Distance) || Distance < 0. || Mode == TriggerMode::Unsupported)
            return { 0., 0., 0., false };
        const double Interval = std::isfinite(FiringRate) && FiringRate >= 0.2 && FiringRate <= 30.
            ? 1. / FiringRate : 0.3;
        if (Mode == TriggerMode::OnPress)
            return { 0.045, 0.065, (std::max)(Interval, 0.1), true };
        if (Mode == TriggerMode::Automatic)
        {
            const double Shots = Precision || Distance > 8000. ? 2. : (Distance > 3500. ? 3. : 5.);
            // Hold through the last planned round, then release before the next
            // native interval. A time cap bounds bursts on slow automatic weapons.
            return { (std::clamp)((Shots - 0.65) * Interval, 0.045, 1.2),
                Precision || Distance > 8000. ? 0.24 : (Distance > 3500. ? 0.18 : 0.12),
                Interval, true };
        }
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
