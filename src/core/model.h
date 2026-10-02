// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace ballistic {
constexpr double Pi = 3.14159265358979323846;
constexpr double EarthRadius = 6371100.0;
constexpr double G0 = 9.80665;
constexpr double ReferenceArea = 4.908738521875;
using State = std::array<double, 4>; // speed, flight-path angle, radius, central angle
enum class ProgramDomain { Bounded, LegacyUnbounded };

struct Parameters {
    double payload = 3000;
    std::array<double, 3> mass{{70480, 29920, 7700}};
    std::array<double, 3> fuel{{60380, 26360, 7346}};
    std::array<double, 3> thrust{{1470000, 392000, 110000}};
    std::array<double, 3> exhaustVelocity{{3297.5, 3498, 3295.6}};
    double verticalTime = 40;
    double turnTime = 310;
    double turnDegrees = 14.97;
    ProgramDomain programDomain = ProgramDomain::Bounded;
    std::array<double, 3> separationTimes() const;
    double duration() const { return separationTimes()[2]; }
    double totalMass() const;
};

struct Options {
    double maxStep = 0.01;
    double targetAltitude = 250000;
    double altitudeTolerance = 100;
    double velocityTolerance = 0.1;
    int maxIterations = 40;
    int maxEvaluations = 250;
    std::size_t maxSteps = 2000000;
    double timeLimitSeconds = 60;
    bool optimize = true;
};

enum class Status { Completed, Cancelled, InvalidInput, NotConverged, NumericalFailure };
enum class StopReason {
    None, Cancelled, InvalidInput, TimeLimit, EvaluationLimit, StepLimit,
    IterationLimit, SensitivityUnavailable, IllConditioned, NoImprovement, NumericalFailure
};
struct Diagnostics {
    StopReason reason = StopReason::None;
    int iterations = 0, rejectedProbes = 0;
    bool residualAvailable = false;
    double altitudeError = 0, velocityError = 0;
};
struct Sample {
    double time = 0, velocity = 0, theta = Pi / 2, radius = EarthRadius, arc = 0;
    double phi = Pi / 2, alpha = 0, mass = 0, acceleration = 0;
    double density = 0, mach = 0, overload = 0;
};
struct Result {
    Status status = Status::InvalidInput;
    std::string message;
    Parameters parameters;
    Options options;
    std::vector<Sample> trajectory;
    int evaluations = 0;
    bool atmosphereClamped = false;
    double elapsedSeconds = 0;
    Diagnostics diagnostics;
};
using CancelCheck = std::function<bool()>;
using Progress = std::function<void(int, double, double)>;
enum class ValidationCode {
    None, PositiveFinite, FuelMass, StageDuration, TotalMass, StartThrust,
    TimeOrder, AngleRange, ProgramRange, StepRange, TargetRange, Limit
};
enum class InputField {
    None, Payload, Mass, Fuel, Thrust, ExhaustVelocity, VerticalTime, TurnTime,
    TurnDegrees, MaxStep, TargetAltitude, AltitudeTolerance, VelocityTolerance,
    MaxIterations, MaxEvaluations, MaxSteps, TimeLimit
};
struct ValidationIssue {
    ValidationCode code = ValidationCode::None;
    InputField field = InputField::None;
    int stage = -1; // Zero-based; -1 for a non-stage field.
    std::string message;
    bool empty() const { return code == ValidationCode::None; }
};
ValidationIssue validateVehicle(const Parameters &p);
ValidationIssue validateProgram(const Parameters &p);
ValidationIssue validateOptions(const Options &o);
ValidationIssue validateDetailed(const Parameters &p, const Options &o);
std::string validate(const Parameters &p, const Options &o);
double programAngle(const Parameters &p, double t);
struct ProgramEnvelope { double minimum, maximum; }; // Radians; valid vehicle/times required.
ProgramEnvelope programEnvelope(const Parameters &p);
double orbitalSpeed(double altitude);
} // namespace ballistic
