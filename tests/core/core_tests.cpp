// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "solver.h"
#include "dynamics.h"
#include "rk4.h"
#include <atomic>
#include <cmath>
#include <future>
#include <limits>
using namespace ballistic;
namespace {
struct ReferenceCase {
    const char *name;
    double payload, verticalTime, turnTime, angle, altitude, velocity, theta, arc, cutoff;
};
const ReferenceCase references[] = {
#include "reference_cases.inc"
};
}

class CoreTests : public QObject {
    Q_OBJECT
    Solver solver;
    Result optimized;
private slots:
    void initTestCase() {
        optimized = solver.run(Parameters{});
        QVERIFY2(optimized.status == Status::Completed, optimized.message.c_str());
    }
    void rkUsesIndependentTime() {
        std::array<double, 1> y{{0}};
        const auto result = rk4(y, 10.0, 0.01, [](double t, const std::array<double, 1> &) {
            return std::array<double, 1>{{t * t}};
        });
        QVERIFY(std::abs(result[0] - (std::pow(10.01, 3) - 1000) / 3) < 1e-12);
    }
    void rkFourthOrder() {
        auto solve = [](double step, int count) {
            std::array<double, 1> y{{1}};
            for (int i = 0; i < count; ++i)
                y = rk4(y, i * step, step, [](double, const std::array<double, 1> &v) { return v; });
            return std::abs(y[0] - std::exp(1.0));
        };
        const double ratio = solve(0.2, 5) / solve(0.1, 10);
        QVERIFY(ratio > 14 && ratio < 18);
    }
    void atmosphereReferenceAndBounds() {
        Atmosphere atmosphere;
        const auto ground = atmosphere.at(0);
        QCOMPARE(ground.temperature, 288.15);
        QCOMPARE(ground.pressure, 101325.0);
        QCOMPARE(ground.density, 1.225);
        QVERIFY(std::abs(ground.soundSpeed - 340.294) < 0.01);
        QVERIFY(std::abs(atmosphere.at(10000).temperature - 223.25) < 0.1);
        const auto middle = atmosphere.at(5500);
        QVERIFY(middle.temperature > 252 && middle.temperature < 253);
        QVERIFY(atmosphere.at(5000).temperature > middle.temperature);
        QVERIFY(atmosphere.at(6000).temperature < middle.temperature);
        QVERIFY(std::abs(atmosphere.at(85999.999).density - atmosphere.at(86000.001).density) < 1e-10);
        QCOMPARE(atmosphere.at(-10).density, ground.density);
        QCOMPARE(atmosphere.at(400000).density, atmosphere.at(300000).density);
        QVERIFY_EXCEPTION_THROWN(atmosphere.at(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    }
    void invalidInputs_data() {
        QTest::addColumn<int>("kind");
        for (int i = 0; i < 10; ++i) QTest::newRow(qPrintable(QString::number(i))) << i;
    }
    void invalidInputs() {
        QFETCH(int, kind);
        Parameters p; Options o;
        switch (kind) {
        case 0: p.thrust[0] = 0; break;
        case 1: p.payload = -1; break;
        case 2: p.fuel[0] = p.mass[0]; break;
        case 3: p.turnTime = p.verticalTime; break;
        case 4: p.turnTime = p.duration(); break;
        case 5: p.turnDegrees = std::numeric_limits<double>::quiet_NaN(); break;
        case 6: o.maxStep = 0; break;
        case 7: o.maxSteps = 0; break;
        case 8: p.exhaustVelocity[2] = std::numeric_limits<double>::infinity(); break;
        case 9: p.thrust[0] = 1; break;
        }
        const auto r = solver.run(p, o);
        QCOMPARE(int(r.status), int(Status::InvalidInput));
        QVERIFY(!r.message.empty()); QVERIFY(r.trajectory.empty());
    }
    void optimizeDefault() {
        qInfo("status=%d; %s; evaluations=%d; seconds=%.3f", int(optimized.status),
              optimized.message.c_str(), optimized.evaluations, optimized.elapsedSeconds);
        QVERIFY2(optimized.status == Status::Completed, optimized.message.c_str());
        const auto &last = optimized.trajectory.back();
        qInfo("points=%llu; altitude=%.9f; speed=%.9f; t1=%.9f; phi1=%.9f",
              static_cast<unsigned long long>(optimized.trajectory.size()), last.radius - EarthRadius,
              last.velocity, optimized.parameters.turnTime, optimized.parameters.turnDegrees);
        QVERIFY(std::abs(last.radius - EarthRadius - 250000) <= 100);
        QVERIFY(std::abs(last.velocity - orbitalSpeed(250000)) <= 0.1);
        const auto &first = optimized.trajectory.front();
        QCOMPARE(first.mass, optimized.parameters.totalMass());
        QVERIFY(first.density > 1); QVERIFY(first.acceleration > 0); QVERIFY(first.overload > 1);
    }
    void samplesAndBoundaries() {
        QVERIFY(optimized.status == Status::Completed);
        const auto times = optimized.parameters.separationTimes();
        std::array<int, 3> count{};
        double previous = -1;
        for (const auto &s : optimized.trajectory) {
            QVERIFY(s.time > previous); previous = s.time;
            QVERIFY(std::isfinite(s.velocity) && std::isfinite(s.alpha) && s.mass > 0);
            QVERIFY(std::abs(s.phi - programAngle(optimized.parameters, s.time)) < 1e-13);
            QVERIFY(std::abs(s.alpha - (s.phi - s.theta + s.arc)) < 1e-13);
            for (int i = 0; i < 3; ++i) if (s.time == times[i]) {
                ++count[i]; double expected = optimized.parameters.payload;
                for (int j = i + 1; j < 3; ++j) expected += optimized.parameters.mass[j];
                QVERIFY(std::abs(s.mass - expected) < 1e-7);
            }
        }
        for (int i = 0; i < 3; ++i) QCOMPARE(count[i], times[i] <= optimized.parameters.duration() ? 1 : 0);
        QCOMPARE(optimized.trajectory.back().time, optimized.parameters.duration());
        QCOMPARE(optimized.trajectory.back().thrust, 0.0);
    }
    void returnedInputsReproduceTrajectory() {
        QVERIFY(optimized.status == Status::Completed);
        Options o; o.optimize = false;
        const auto replay = solver.run(optimized.parameters, o);
        QVERIFY(replay.status == Status::Completed);
        QCOMPARE(replay.trajectory.size(), optimized.trajectory.size());
        for (std::size_t i = 0; i < replay.trajectory.size(); i += 100) {
            QCOMPARE(replay.trajectory[i].radius, optimized.trajectory[i].radius);
            QCOMPARE(replay.trajectory[i].velocity, optimized.trajectory[i].velocity);
        }
        QCOMPARE(replay.trajectory.back().radius, optimized.trajectory.back().radius);
    }
    void smallerStepsAgree() {
        QVERIFY(optimized.status == Status::Completed);
        Options o; o.optimize = false; o.maxStep = 0.005;
        const auto fine = solver.run(optimized.parameters, o);
        QVERIFY2(fine.status == Status::Completed, fine.message.c_str());
        const auto &a = fine.trajectory.back(), &b = optimized.trajectory.back();
        qInfo("refinement: deltaH=%.9f, deltaV=%.9f", a.radius - b.radius, a.velocity - b.velocity);
        QVERIFY(std::abs(a.radius - b.radius) < 5);
        QVERIFY(std::abs(a.velocity - b.velocity) < 0.05);
    }
    void independentConcurrentRuns() {
        Options o; o.optimize = false;
        Parameters a, b; b.payload += 10;
        auto future = std::async(std::launch::async, [&] { return solver.run(a, o); });
        const auto rb = solver.run(b, o);
        const auto ra = future.get();
        const auto repeat = solver.run(a, o);
        QVERIFY(ra.status == Status::Completed && rb.status == Status::Completed);
        QCOMPARE(ra.trajectory.back().velocity, repeat.trajectory.back().velocity);
        QVERIFY(ra.trajectory.back().velocity != rb.trajectory.back().velocity);
    }
    void cancellationAndLimits() {
        QCOMPARE(int(solver.run(Parameters{}, Options{}, [] { return true; }).status), int(Status::Cancelled));
        bool stop = false;
        const auto mid = solver.run(Parameters{}, Options{}, [&] { return stop; },
                                   [&](int, double, double) { stop = true; });
        QCOMPARE(int(mid.status), int(Status::Cancelled));
        QVERIFY(mid.trajectory.empty());
        Options o; o.maxSteps = 2;
        QCOMPARE(int(solver.run(Parameters{}, o).status), int(Status::NotConverged));
        o = Options{}; o.maxIterations = 1;
        QCOMPARE(int(solver.run(Parameters{}, o).status), int(Status::NotConverged));
        o = Options{}; o.timeLimitSeconds = 1e-9;
        QCOMPARE(int(solver.run(Parameters{}, o).status), int(Status::NotConverged));
    }
    void minimumProgramInterval() {
        Parameters p; p.turnTime = p.verticalTime + 0.001;
        QVERIFY(validate(p, Options{}).empty());
        p.turnTime = p.verticalTime + 0.000999;
        QVERIFY(!validate(p, Options{}).empty());
    }
    void derivedStageDurationMustBePositive() {
        Parameters p; p.fuel[0] = 1e-300; p.exhaustVelocity[0] = 1e-300;
        QVERIFY(!validate(p, Options{}).empty());
    }
    void structuredValidation() {
        Parameters p; Options o;
        p.fuel[1] = p.mass[1];
        auto error = validateVehicle(p);
        QCOMPARE(error.code, ValidationCode::FuelMass); QCOMPARE(error.field, InputField::Fuel); QCOMPARE(error.stage, 1);
        p = Parameters{}; p.turnTime = p.verticalTime;
        QVERIFY(validateVehicle(p).empty());
        QCOMPARE(validateProgram(p).field, InputField::TurnTime);
        p = Parameters{}; p.thrust[0] = 900000;
        QCOMPARE(validateVehicle(p).code, ValidationCode::StartThrust);
        p = Parameters{}; p.mass[0] = p.mass[1] = std::numeric_limits<double>::max();
        QCOMPARE(validateVehicle(p).code, ValidationCode::TotalMass);
        p = Parameters{}; p.exhaustVelocity[0] = 1e-308;
        QCOMPARE(validateVehicle(p).code, ValidationCode::StageDuration);
        p = Parameters{}; p.turnDegrees = 0; p.turnTime = p.duration() - 0.001;
        QVERIFY(validateProgram(p).empty());
        p.turnTime = p.duration() - 0.000999;
        QVERIFY(!validateProgram(p).empty());
        o.velocityTolerance = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(validateOptions(o).field, InputField::VelocityTolerance);
        o = Options{}; o.maxEvaluations = 0;
        QCOMPARE(validateOptions(o).field, InputField::MaxEvaluations);
        o = Options{}; o.maxStep = 1e-6; QVERIFY(validateOptions(o).empty());
        o.maxStep = 1; QVERIFY(validateOptions(o).empty());
    }
    void preparedModelAndFixedRegression() {
        Parameters p;
        const PreparedModel model(p);
        for (double time : {0.0, 40.0, 100.0, 310.0, 500.0, p.duration()})
            QCOMPARE(model.angle(time), programAngle(p, time));
        const auto times = model.separationTimes();
        QCOMPARE(model.stageAt(times[0]), 1);
        QCOMPARE(model.mass(times[0], 1), p.payload + p.mass[1] + p.mass[2]);
        QCOMPARE(model.mass(times[2], 3), p.payload);
        QVERIFY_EXCEPTION_THROWN(model.mass(0, -1), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(model.mass(0, 4), std::invalid_argument);
        p.payload = 4000; QCOMPARE(model.parameters().payload, 3000.0);
        p = Parameters{}; p.turnTime = 353.527598424922; p.turnDegrees = 9.07661185847695;
        Options o; o.optimize = false;
        const auto r = solver.run(p, o);
        QCOMPARE(r.status, Status::Completed);
        QVERIFY(std::abs(r.trajectory.back().radius - EarthRadius - 249999.821605668) < 1e-5);
        QVERIFY(std::abs(r.trajectory.back().velocity - 7753.71272168105) < 1e-7);
    }
    void invalidSensitivityProbeDoesNotAbort() {
        Parameters p; p.turnTime = 580; p.turnDegrees = 12.985755371093751;
        p.programDomain = ProgramDomain::LegacyUnbounded;
        Options o; o.maxStep = 0.1; o.optimize = false;
        QCOMPARE(solver.run(p, o).status, Status::Completed);
        o.optimize = true;
        const auto r = solver.run(p, o);
        QVERIFY(r.status != Status::NumericalFailure);
        QVERIFY(r.diagnostics.rejectedProbes > 0);
        QVERIFY(r.evaluations > 2);
        QVERIFY(r.diagnostics.residualAvailable);
    }
    void optimizerDiagnosticsAndLimits() {
        Options o; o.maxEvaluations = 2;
        auto r = solver.run(Parameters{}, o);
        QCOMPARE(r.diagnostics.reason, StopReason::EvaluationLimit); QCOMPARE(r.evaluations, 2);
        o = Options{}; o.maxSteps = 2;
        QCOMPARE(solver.run(Parameters{}, o).diagnostics.reason, StopReason::StepLimit);
        o = Options{}; o.maxIterations = 1;
        QCOMPARE(solver.run(Parameters{}, o).diagnostics.reason, StopReason::IterationLimit);
        o = Options{}; o.timeLimitSeconds = 1e-9;
        QCOMPARE(solver.run(Parameters{}, o).diagnostics.reason, StopReason::TimeLimit);
        QCOMPARE(solver.run(Parameters{}, Options{}, [] { return true; }).diagnostics.reason, StopReason::Cancelled);
        Parameters p; p.turnTime = 580; p.turnDegrees = 12.985755371093751;
        p.programDomain = ProgramDomain::LegacyUnbounded;
        o = Options{}; o.maxStep = 0.1; o.maxEvaluations = 2;
        r = solver.run(p, o);
        QCOMPARE(r.diagnostics.reason, StopReason::EvaluationLimit);
        QCOMPARE(r.evaluations, 2); QCOMPARE(r.diagnostics.rejectedProbes, 1);
    }
    void independentReferenceMatrix_data() {
        QTest::addColumn<int>("scenario"); QTest::addColumn<double>("step");
        for (int i = 0; i < int(sizeof(references) / sizeof(references[0])); ++i)
            for (double step : {1.0, 0.05, 0.01, 0.005})
                QTest::newRow(qPrintable(QString("%1-step-%2").arg(references[i].name).arg(step))) << i << step;
    }
    void independentReferenceMatrix() {
        QFETCH(int, scenario); QFETCH(double, step);
        const auto &reference = references[scenario];
        Parameters p; p.payload = reference.payload; p.verticalTime = reference.verticalTime;
        p.turnTime = reference.turnTime; p.turnDegrees = reference.angle;
        p.engineCutoffTime = reference.cutoff;
        Options o; o.optimize = false; o.maxStep = step;
        const auto r = solver.run(p, o); QVERIFY2(r.status == Status::Completed, r.message.c_str());
        const auto &last = r.trajectory.back();
        qInfo("reference delta H=%.9g m, V=%.9g m/s, theta=%.9g rad, arc=%.9g rad",
              last.radius - EarthRadius - reference.altitude, last.velocity - reference.velocity,
              last.theta - reference.theta, last.arc - reference.arc);
        QVERIFY(std::abs(last.radius - EarthRadius - reference.altitude) < 0.05);
        QVERIFY(std::abs(last.velocity - reference.velocity) < 0.0001);
        QVERIFY(std::abs(last.theta - reference.theta) < 1e-7);
        QVERIFY(std::abs(last.arc - reference.arc) < 1e-8);
        if (reference.altitude > 300000) QVERIFY(r.atmosphereClamped);
    }
    void analyticRocketAndCircularMotion() {
        std::array<double, 2> rocket{{0, 10000}};
        for (int i = 0; i < 1000; ++i)
            rocket = rk4(rocket, i * .05, .05, [](double, const std::array<double, 2> &s) {
                return std::array<double, 2>{{3000 * 10 / s[1], -10}};
            });
        QVERIFY(std::abs(rocket[0] - 3000 * std::log(10000.0 / 9500)) < 1e-9);
        std::array<double, 4> orbit{{1, 0, 0, 1}};
        const double step = 2 * Pi / 2000;
        for (int i = 0; i < 2000; ++i)
            orbit = rk4(orbit, i * step, step, [](double, const std::array<double, 4> &s) {
                const double r = std::hypot(s[0], s[1]);
                return std::array<double, 4>{{s[2], s[3], -s[0] / (r*r*r), -s[1] / (r*r*r)}};
            });
        QVERIFY(std::hypot(orbit[0] - 1, orbit[1]) < 1e-8);
        const double energy = (orbit[2]*orbit[2] + orbit[3]*orbit[3]) / 2 - 1 / std::hypot(orbit[0], orbit[1]);
        QVERIFY(std::abs(energy + .5) < 1e-10);
    }
    void coincidentEventsAndProgramDomain() {
        Parameters p; p.verticalTime = p.separationTimes()[0]; p.turnTime = p.separationTimes()[1]; p.turnDegrees = 10;
        Options o; o.optimize = false; o.maxStep = .1;
        const auto r = solver.run(p, o); QVERIFY2(r.status == Status::Completed, r.message.c_str());
        for (double event : p.separationTimes()) {
            const auto count = std::count_if(r.trajectory.begin(), r.trajectory.end(), [=](const Sample &s) { return s.time == event; });
            QCOMPARE(int(count), 1);
        }
        p = Parameters{}; p.turnTime = p.duration() - .01;
        QVERIFY(programEnvelope(p).maximum > 1000);
        QCOMPARE(validateProgram(p).code, ValidationCode::ProgramRange);
        QCOMPARE(solver.run(p, o).status, Status::InvalidInput);
        p.programDomain = ProgramDomain::LegacyUnbounded; QVERIFY(validateProgram(p).empty());
        p = Parameters{};
        const auto range = programEnvelope(p); QCOMPARE(range.minimum, 0.0); QCOMPARE(range.maximum, Pi / 2);
    }
    void optimizationTargetMatrix_data() {
        QTest::addColumn<double>("target");
        for (double target : {180000.0, 200000.0, 220000.0, 240000.0, 250000.0, 260000.0, 300000.0})
            QTest::newRow(qPrintable(QString::number(target))) << target;
    }
    void optimizationTargetMatrix() {
        QFETCH(double, target); Options o; o.targetAltitude = target;
        const auto r = solver.run(Parameters{}, o); QVERIFY2(r.status == Status::Completed, r.message.c_str());
        QVERIFY(std::abs(r.trajectory.back().radius - EarthRadius - target) <= o.altitudeTolerance);
        QVERIFY(std::abs(r.trajectory.back().velocity - orbitalSpeed(target)) <= o.velocityTolerance);
        QVERIFY(validateProgram(r.parameters).empty());
        qInfo("target=%.0f; H=%.6f; V=%.6f; cutoff=%.9f; fuel=%.6f; mass=%.6f; iterations=%d; evaluations=%d",
              target, r.trajectory.back().radius - EarthRadius, r.trajectory.back().velocity,
              r.parameters.duration(), r.parameters.remainingFuel(), r.trajectory.back().mass,
              r.diagnostics.iterations, r.evaluations);
        if (target <= 200000) {
            QVERIFY(r.parameters.engineCutoffTime < r.parameters.separationTimes()[2]);
            QVERIFY(r.parameters.remainingFuel() > 0);
            QCOMPARE(r.trajectory.back().time, r.parameters.engineCutoffTime);
            QVERIFY(std::abs(r.trajectory.back().mass - (r.parameters.payload + r.parameters.mass[2] -
                          r.parameters.fuel[2] + r.parameters.remainingFuel())) < 1e-8);
            o.optimize = false;
            const auto replay = solver.run(r.parameters, o);
            QCOMPARE(replay.status, Status::Completed);
            QCOMPARE(replay.trajectory.back().radius, r.trajectory.back().radius);
            QCOMPARE(replay.trajectory.back().velocity, r.trajectory.back().velocity);
            o.maxStep = 0.005;
            const auto fine = solver.run(r.parameters, o);
            QCOMPARE(fine.status, Status::Completed);
            QVERIFY(std::abs(fine.trajectory.back().radius - r.trajectory.back().radius) < 0.05);
            QVERIFY(std::abs(fine.trajectory.back().velocity - r.trajectory.back().velocity) < 0.0001);
        }
    }
    void earlyCutoffMassAndEvent() {
        Parameters p; p.engineCutoffTime = 570.123456789;
        const auto times = p.separationTimes();
        const double remaining = p.fuel[2] - (p.engineCutoffTime - times[1]) * p.thrust[2] / p.exhaustVelocity[2];
        const double mass = p.payload + p.mass[2] - p.fuel[2] + remaining;
        Atmosphere air; Dynamics dynamics(p, air);
        QCOMPARE(dynamics.stageAt(p.duration()), 3);
        QVERIFY(std::abs(dynamics.mass(p.duration(), 3) - mass) < 1e-8);
        QCOMPARE(dynamics.mass(p.duration() + 100, 3), dynamics.mass(p.duration(), 3));
        QVERIFY(std::abs(p.remainingFuel() - remaining) < 1e-8);
        Options o; o.optimize = false; o.maxStep = 0.1;
        const auto r = solver.run(p, o); QVERIFY2(r.status == Status::Completed, r.message.c_str());
        const auto &last = r.trajectory.back();
        QCOMPARE(last.time, p.engineCutoffTime); QCOMPARE(last.thrust, 0.0);
        QCOMPARE(last.phi, 0.0); QVERIFY(std::abs(last.mass - mass) < 1e-8);
        const auto &previous = r.trajectory[r.trajectory.size() - 2];
        QVERIFY(previous.thrust > 0);
        QVERIFY(std::abs(previous.mass - (last.time - previous.time) * p.thrust[2] / p.exhaustVelocity[2] - last.mass) < 1e-8);
        int cutoffs = 0;
        for (const auto &sample : r.trajectory) {
            QVERIFY(sample.time <= p.duration());
            if (sample.time == p.duration()) ++cutoffs;
        }
        QCOMPARE(cutoffs, 1);
        p.engineCutoffTime = times[2];
        const auto full = solver.run(p, o);
        QCOMPARE(full.status, Status::Completed);
        QCOMPARE(full.trajectory.back().mass, p.payload); QCOMPARE(p.remainingFuel(), 0.0);
        p.engineCutoffTime = 0;
        const auto legacy = solver.run(p, o);
        QCOMPARE(full.trajectory.back().velocity, legacy.trajectory.back().velocity);
    }
    void invalidCutoffTimes() {
        Parameters p; Options o;
        for (double time : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                            p.separationTimes()[1], p.separationTimes()[1] + 0.0005, p.separationTimes()[2] + 0.001}) {
            p.engineCutoffTime = time;
            QCOMPARE(validateProgram(p).field, InputField::EngineCutoffTime);
            const auto r = solver.run(p, o);
            QCOMPARE(r.status, Status::InvalidInput); QCOMPARE(r.evaluations, 0);
        }
    }
};
QTEST_APPLESS_MAIN(CoreTests)
#include "core_tests.moc"
