using System;
using System.Linq;
using System.Collections.Generic;
using System.ComponentModel;

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
        }

        public void RunRungeKutta()
        {
            results.Clear();        
            calculationVector = new CalculationVector();
            calculationVector.Tetta = Math.PI/2;
            calculationVector.Radius = CalculationVector.RadiusOfEarth;
            var bw = new BackgroundWorker();
            bw.DoWork += DoWork;
            bw.RunWorkerCompleted += BwCompleted;
            bw.RunWorkerAsync();
        }

        public event Action<List<CalculationVector>, double, double> WorkCompleted;

        protected virtual void OnWorkCompleted()
        {
            var handler = WorkCompleted;
            if (handler != null) handler(results, initialParams.Phi1Time, initialParams.Phi1*180/Math.PI);
        }

        private void BwCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            OnWorkCompleted();
        }

        private void DoWork(object sender, DoWorkEventArgs e)
        {
            var step = calculationVector;
            var phiStart = initialParams.Phi1;
            var deltaPhi = 0.1 * 3.141592654 / 180.0;
            var resultAltitude = 0.0;
            var resultVelocity = 0.0;
            var deltaTime = - 10.0;
            var resultPhi = 0.0;
            var neededVelocyty = Math.Sqrt(CalculationVector.G0 * Math.Pow(CalculationVector.RadiusOfEarth, 2) / (250000.0 + CalculationVector.RadiusOfEarth));
            while (Math.Abs(resultVelocity - neededVelocyty) > 0.1)
            {
                results.Clear();
                while (Math.Abs(resultAltitude - 250.0) > 0.1)
                {
                    results.Clear();
                    while (step.CurrentTime < initialParams.TimeSumm)
                    {
                        results.Add(step);
                        var dtCorrected = step.Velocity * dt < 10 ? dt : 10 / step.Velocity;
                        step = CalculationVector.GetNewStep(step, dtCorrected);
                    }
                    var newResult = (results.Last().Radius - CalculationVector.RadiusOfEarth) / 1000.0;
                    if (resultAltitude < 1.0)
                    {
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
                            deltaTime = Math.Abs(deltaTime)/deltaTime * 30.0;
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
        }
    }
}
