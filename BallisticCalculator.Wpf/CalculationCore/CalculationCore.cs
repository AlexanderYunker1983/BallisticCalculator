using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace CalculationCore
{
    public class CalculationCore
    {
        private readonly AtmosphereInterpolator atmosphereInterpolator = new AtmosphereInterpolator();
        private InitialParams initialParams;
        private CalculationVector calculationVector;
        private readonly List<CalculationVector> results = new List<CalculationVector>();
        private double dt;

        public CalculationCore()
        {
            CalculationVector.Atmosphere = atmosphereInterpolator;
        }

        public void SetInitParams(InitialParams initParams, double deltTime)
        {
            initialParams = initParams;
            dt = deltTime;
            CalculationVector.InParams = initialParams;
            CalculationVector.A = initialParams.A;
            CalculationVector.B = initParams.B;
            CalculationVector.C = initParams.C;

            // Грубая оценка количества шагов интегрирования для предвыделения списка
            if (dt > 0 && initialParams.TimeSumm > 0)
            {
                var estimatedSteps = (int)(initialParams.TimeSumm / dt) + 1;
                if (estimatedSteps > results.Capacity)
                {
                    results.Capacity = estimatedSteps;
                }
            }
        }

        /// <summary>
        /// Запускает расчёт траектории в пуле потоков и возвращает результат через Task.
        /// </summary>
        public Task<CalculationResult> RunAsync(CancellationToken cancellationToken)
        {
            return Task.Run(() => RunCore(cancellationToken), cancellationToken);
        }

        /// <summary>
        /// Основное ядро расчёта (бывший DoWork), выполняется синхронно.
        /// </summary>
        private CalculationResult RunCore(CancellationToken cancellationToken)
        {
            results.Clear();
            calculationVector = new CalculationVector
            {
                Tetta = Math.PI / 2,
                Radius = CalculationVector.RadiusOfEarth,
                Phi = Math.PI / 2 // программа запуска: 90° в момент t = 0
            };

            var step = calculationVector;
            var phiStart = initialParams.Phi1;
            var deltaPhi = 0.1 * 3.141592654 / 180.0;
            var resultAltitude = 0.0;
            var resultVelocity = 0.0;
            var deltaTime = -10.0;
            var resultPhi = 0.0;
            var neededVelocyty = Math.Sqrt(CalculationVector.G0 * Math.Pow(CalculationVector.RadiusOfEarth, 2) /
                                           (250000.0 + CalculationVector.RadiusOfEarth));

            while (Math.Abs(resultVelocity - neededVelocyty) > 0.1)
            {
                cancellationToken.ThrowIfCancellationRequested();

                results.Clear();
                while (Math.Abs(resultAltitude - 250.0) > 0.1)
                {
                    cancellationToken.ThrowIfCancellationRequested();

                    results.Clear();

                    while (step.CurrentTime < initialParams.TimeSumm)
                    {
                        cancellationToken.ThrowIfCancellationRequested();

                        results.Add(step);

                        // Базовая адаптация шага по скорости
                        double dtCorrected = step.Velocity * dt < 10 ? dt : 10 / step.Velocity;

                        // Дополнительная адаптация вокруг моментов разделения ступеней:
                        // делаем шаг так, чтобы он не "перепрыгивал" через Time1/TimeSumm2/TimeSumm3,
                        // а заканчивался точно в момент разделения.
                        var nextSeparationTime = GetNextSeparationTime(step.CurrentTime);
                        if (nextSeparationTime > step.CurrentTime &&
                            step.CurrentTime + dtCorrected > nextSeparationTime)
                        {
                            dtCorrected = nextSeparationTime - step.CurrentTime;
                        }

                        step = CalculationVector.GetNewStep(step, dtCorrected);
                    }

                    // гарантируем, что последняя точка траектории всегда сохранена
                    if (results.Count == 0 || !ReferenceEquals(results[^1], step))
                    {
                        results.Add(step);
                    }

                    var newResult = (results.Last().Radius - CalculationVector.RadiusOfEarth) / 1000.0;
                    if (resultAltitude < 1.0)
                    {
                        // первая итерация — нет истории для корректировки
                    }
                    else
                    {
                        var deltaOld = Math.Abs(resultAltitude - 250.0);
                        var deltaNew = Math.Abs(newResult - 250.0);
                        var singn = deltaOld / deltaNew > 1 ? 1.0 : -1.0;
                        deltaPhi = singn * deltaPhi * deltaNew / Math.Abs(deltaOld - deltaNew);
                        if (Math.Abs(deltaPhi) > 10.0 * Math.PI / 180)
                        {
                            deltaPhi = Math.Abs(deltaPhi) / deltaPhi * 10.0 * Math.PI / 180;
                        }
                    }

                    initialParams.Phi1 -= deltaPhi;
                    SetInitParams(initialParams, dt);
                    step = calculationVector;
                    resultAltitude = newResult;
                }

                var newVelocity = results.Last().Velocity;
                if (resultVelocity < 1.0)
                {
                    // первая итерация — нет истории для корректировки
                }
                else
                {
                    var deltaOld = Math.Abs(resultVelocity - neededVelocyty);
                    var deltaNew = Math.Abs(newVelocity - neededVelocyty);
                    if (Math.Abs(deltaOld - deltaNew) > 1.0)
                    {
                        var singn = deltaOld / deltaNew > 1 ? 1.0 : -1.0;
                        deltaTime = singn * deltaTime * deltaNew / Math.Abs(deltaOld - deltaNew);
                        if (Math.Abs(deltaTime) > 30.0)
                        {
                            deltaTime = Math.Abs(deltaTime) / deltaTime * 30.0;
                        }
                    }
                }

                initialParams.Phi1Time -= deltaTime;
                resultPhi = initialParams.Phi1;
                initialParams.Phi1 = phiStart;
                SetInitParams(initialParams, dt);
                step = calculationVector;
                resultVelocity = newVelocity;
                resultAltitude = 0.0;
                deltaPhi = 0.1 * 3.141592654 / 180.0;
            }

            initialParams.Phi1 = resultPhi;

            // Возвращаем полный набор точек и найденные оптимальные параметры
            return new CalculationResult(
                Trajectory: new List<CalculationVector>(results),
                Phi1Time: initialParams.Phi1Time,
                Phi1Degrees: initialParams.Phi1 * 180 / Math.PI);
        }

        /// <summary>
        /// Возвращает ближайший момент разделения ступеней (Time1, TimeSumm2, TimeSumm3)
        /// после текущего времени, либо -1, если таких больше нет.
        /// </summary>
        private double GetNextSeparationTime(double currentTime)
        {
            var next = double.PositiveInfinity;

            void Consider(double t)
            {
                if (t > currentTime && t < next)
                {
                    next = t;
                }
            }

            Consider(initialParams.Time1);
            Consider(initialParams.TimeSumm2);
            Consider(initialParams.TimeSumm3);

            return double.IsPositiveInfinity(next) ? -1.0 : next;
        }
    }
}
