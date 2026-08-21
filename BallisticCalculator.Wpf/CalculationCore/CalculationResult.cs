using System.Collections.Generic;

namespace CalculationCore;

/// <summary>
/// Результат баллистического расчёта.
/// </summary>
public sealed record CalculationResult(
    IReadOnlyList<CalculationVector> Trajectory,
    double Phi1Time,
    double Phi1Degrees);

