using System;

namespace CalculationCore
{
    public class CalculationVector
    {
        public double Velocity;
        public double Tetta;
        public double Phi;
        public double CurrentTime;
        public double Radius;
        public double Hi;
        public double Alpha;
        public double Massa;
        public double Acceleration;
        public double Density;
        public double Mah;
        public const double RadiusOfEarth = 6371100.0;
        public static AtmosphereInterpolator Atmosphere;
        public const double Surf = 4.908738521875;
        public const double G0 = 9.80665;
        public static InitialParams InParams;
        public static double A;
        public static double B;
        public static double C;

        public static CalculationVector operator +(CalculationVector a, CalculationVector b)
        {
            var opAddition = new CalculationVector
            {
                Alpha = a.Alpha + b.Alpha,
                CurrentTime = a.CurrentTime + b.CurrentTime,
                Hi = a.Hi + b.Hi,
                Radius = a.Radius + b.Radius,
                Tetta = a.Tetta + b.Tetta,
                Velocity = a.Velocity + b.Velocity
            };
            opAddition.Mah = opAddition.Velocity/ Atmosphere.GetSoundVelocity(opAddition.Radius - RadiusOfEarth);
            return opAddition;
        }

        public static CalculationVector operator -(CalculationVector a, CalculationVector b)
        {
            var opSubtraction = new CalculationVector
            {
                Alpha = a.Alpha - b.Alpha,
                CurrentTime = a.CurrentTime - b.CurrentTime,
                Hi = a.Hi - b.Hi,
                Radius = a.Radius - b.Radius,
                Tetta = a.Tetta - b.Tetta,
                Velocity = a.Velocity - b.Velocity,
            };
            opSubtraction.Mah = opSubtraction.Velocity / Atmosphere.GetSoundVelocity(opSubtraction.Radius - RadiusOfEarth);
            return opSubtraction;
        }

        public static CalculationVector operator *(double a, CalculationVector b)
        {
            var opMultiply = new CalculationVector
            {
                Alpha = a * b.Alpha,
                CurrentTime = a * b.CurrentTime,
                Hi = a * b.Hi,
                Radius = a * b.Radius,
                Tetta = a * b.Tetta,
                Velocity = a * b.Velocity,
            };
            opMultiply.Mah = opMultiply.Velocity / Atmosphere.GetSoundVelocity(opMultiply.Radius - RadiusOfEarth);
            return opMultiply;
        }

        public static CalculationVector operator *(CalculationVector b,double a)
        {
            var opMultiply = new CalculationVector
            {
                Alpha = a * b.Alpha,
                CurrentTime = a * b.CurrentTime,
                Hi = a * b.Hi,
                Radius = a * b.Radius,
                Tetta = a * b.Tetta,
                Velocity = a * b.Velocity,
            };
            opMultiply.Mah = opMultiply.Velocity / Atmosphere.GetSoundVelocity(opMultiply.Radius - RadiusOfEarth);
            return opMultiply;
        }

        public static CalculationVector F(CalculationVector u, double t)
        {
            var calculationVector = new CalculationVector
            {
                Velocity = GetVelocityDx(u),
                Tetta = GetTettaDx(u),
                Radius = GetRadiusDx(u),
                Hi = GetHiDx(u),
                CurrentTime = t,
                Alpha = 0
            };
            calculationVector.Mah = calculationVector.Velocity / Atmosphere.GetSoundVelocity(calculationVector.Radius - RadiusOfEarth);
            return calculationVector;
        }

        public static CalculationVector GetNewStep(CalculationVector u, double dt)
        {
            var t = u.CurrentTime;
            var k1 = F(u, t)*dt;
            var k2 = F(u + 0.5*k1, t + 0.5*dt)*dt;
            var k3 = F(u + 0.5*k2, t + 0.5*dt)*dt;
            var k4 = F(u + k3, t + dt)*dt;
            var result = u + 1.0/6.0*(k1 + 2.0*k2 + 2.0*k3 + k4);
            result.CurrentTime = t + dt;
            result.Phi = GetPhi(result.CurrentTime);
            result.Alpha = GetAlpha(result);
            result.Massa = GetMass(result.CurrentTime);
            result.Acceleration = (result.Velocity - u.Velocity)/dt;
            result.Density = Atmosphere.GetDensity(result.Radius - RadiusOfEarth);
            return result;
        }

        private static double GetAlpha(CalculationVector u)
        {
            return GetPhi(u.CurrentTime) - u.Tetta + u.Hi;
        }

        private static double GetPhi(double currentTime)
        {
            if (currentTime <= InParams.Phi0Time) return Math.PI/2.0;
            if (currentTime >= InParams.TimeSumm) return 0;
            if (currentTime >= InParams.Phi1Time)
                return (InParams.TimeSumm - currentTime) /
                       (InParams.TimeSumm - InParams.Phi1Time) * InParams.Phi1;
//             return InParams.Phi1 + (InParams.Phi1Time - currentTime) /
//                        (InParams.Phi1Time - InParams.Phi0Time) * (Math.PI/2 - InParams.Phi1);
            return A*Math.Pow(currentTime, 2) + B*currentTime + C;
        }

        private static double GetMass(double currentTime)
        {
            if (currentTime <= InParams.Time1)
            {
                return InParams.SummMass - currentTime*InParams.Consumption1;
            }
            if (currentTime <= InParams.TimeSumm2)
            {
                return InParams.SummMass2 - currentTime * InParams.Consumption2;
            }
            if (currentTime <= InParams.TimeSumm3)
            {
                return InParams.SummMass3 - currentTime * InParams.Consumption3;
            }
            return InParams.MassGo;
        }

        private static double GetThrust(double currentTime)
        {
            if (currentTime <= InParams.Time1) return InParams.Thrust1;
            if (currentTime <= InParams.TimeSumm2) return InParams.Thrust2;
            return currentTime <= InParams.TimeSumm3 ? InParams.Thrust3 : 0.0;
        }

        private static double GetHiDx(CalculationVector calculationVector)
        {
            return calculationVector.Velocity / calculationVector.Radius * Math.Cos(calculationVector.Tetta);
        }

        private static double GetRadiusDx(CalculationVector calculationVector)
        {
            return calculationVector.Velocity * Math.Sin(calculationVector.Tetta);
        }

        private static double GetTettaDx(CalculationVector u)
        {
            if (Math.Abs(u.Velocity) <= 0.5)
            {
                return 0;
            }
            return (1.0/(GetMass(u.CurrentTime)*u.Velocity))*
                   ((GetThrust(u.CurrentTime) -
                     Atmosphere.GetDensity(u.Radius - RadiusOfEarth)*Math.Pow(u.Velocity, 2.0)/2.0*Surf*GetCx(u))*
                    Math.Sin(u.Alpha) +
                    Atmosphere.GetDensity(u.Radius - RadiusOfEarth)*Math.Pow(u.Velocity, 2.0)/2.0*Surf*GetCy(u)*
                    Math.Cos(u.Alpha)) -
                   Math.Cos(u.Tetta)*(GetG(u)/u.Velocity - u.Velocity/u.Radius);
        }

        public static double GetG(CalculationVector u)
        {
            return G0*RadiusOfEarth*RadiusOfEarth/(u.Radius*u.Radius);
        }

        public static double GetCx(CalculationVector u)
        {
            var mah = u.Mah;
            if (mah <= 0.8) return 0.29;
            if (mah <= 1.068) return mah - 0.51;
            return 0.089 + 0.5/mah;
        }

        public static double GetCy(CalculationVector u)
        {
            var cx = GetCx(u);
            var mah = u.Mah;
            double cya;
            if (mah <= 0.25) cya = 2.8;
            else if (mah <= 1.1) cya = 2.8 + 0.447 * (mah - 0.25);
            else if (mah <= 1.6) cya = 3.18 - 0.660 * (mah - 1.1);
            else if (mah <= 3.6) cya = 2.85 + 0.350*(mah - 1.6);
            else cya = 3.55;
            return (cya - cx)*u.Alpha;
        }

        private static double GetVelocityDx(CalculationVector u)
        {
            return (1.0/GetMass(u.CurrentTime))*
                   ((GetThrust(u.CurrentTime) -
                     Math.Pow(u.Velocity, 2.0)*Atmosphere.GetDensity(u.Radius - RadiusOfEarth)/2.0*GetCx(u)*Surf)*
                    Math.Cos(u.Alpha) -
                    GetCy(u)*Math.Sin(u.Alpha)*Atmosphere.GetDensity(u.Radius - RadiusOfEarth)*Math.Pow(u.Velocity, 2.0)/
                    2.0*Surf) - GetG(u)*Math.Sin(u.Tetta);
        }
    }
}
