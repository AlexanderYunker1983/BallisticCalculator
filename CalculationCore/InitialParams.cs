using System;

namespace CalculationCore
{
    public struct InitialParams
    {
        public double MassGo;
        public double Mass1;
        public double Mass2;
        public double Mass3; 
        public double MassFuel1;
        public double MassFuel2;
        public double MassFuel3;
        public double Thrust1;
        public double Thrust2;
        public double Thrust3;
        public double Isp1;
        public double Isp2;
        public double Isp3;
        public double Time1 { get { return MassFuel1 * Isp1 / Thrust1; } }
        public double Time2 { get { return MassFuel2 * Isp2 / Thrust2; } }
        public double Time3 { get { return MassFuel3 * Isp3 / Thrust3; } }
        public double TimeSumm { get { return Time1 + Time2 + Time3; } }
        public double Phi0Time;
        public double Phi1Time;
        public double Phi1;
        
        public double A { get
        {
            return (Math.PI/2.0 - Phi1 + Phi1*(Phi0Time - Phi1Time)/(TimeSumm - Phi1Time))/
                   (Math.Pow(Phi0Time, 2) - Math.Pow(Phi1Time, 2) - 2.0*Phi1Time*(Phi0Time - Phi1Time));
        } }

        public double B { get { return - 1.0 * Phi1/(TimeSumm - Phi1Time) - 2*A*Phi1Time; } }

        public double C { get { return Math.PI/2 - A*Math.Pow(Phi0Time, 2) - B*Phi0Time; } }

        public double SummMass;
        public double SummMass2;
        public double SummMass3;

        public double Consumption1;
        public double Consumption2;
        public double Consumption3;

        public double TimeSumm2;
        public double TimeSumm3;

        public void Initialize()
        {
            SummMass = MassGo + Mass3 + Mass2 + Mass1;
            Consumption1 = Thrust1 / Isp1;
            Consumption2 = Thrust2 / Isp2;
            Consumption3 = Thrust3 / Isp3;

            SummMass2 = SummMass - Mass1 + Time1 * Consumption2;
            SummMass3 = SummMass - Mass1 - Mass2 + (Time1 + Time2) * Consumption3;

            TimeSumm2 = Time2 + Time1;
            TimeSumm3 = TimeSumm2 + Time3;
        }
    }
}
