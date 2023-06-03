using System;

namespace CalculationCore
{
    /// <summary>
    /// Параметры стандартной атмосферы на заданной высоте
    /// </summary>
    public class AtmosphereParams
    {
        /// <summary>
        /// Высота над уровнем моря, км
        /// </summary>
        public double Altitude { get; private set; }
        /// <summary>
        /// Плотность, кг/м^3
        /// </summary>
        public double Density { get; private set; }
        /// <summary>
        /// Давление, Па
        /// </summary>
        public double Presure { get; private set; }
        /// <summary>
        /// Давление, мм.рт.ст.
        /// </summary>
        public double PresureInTorr { get { return Presure / 133.322; } }
        /// <summary>
        /// Температура, К
        /// </summary>
        public double Temperature { get; private set; }
        /// <summary>
        /// Скорость звука, м/с
        /// </summary>
        public double SoundVelocity { get; private set; }

        /// <summary>
        /// Конструктор по-умолчанию
        /// </summary>
        /// <param name="altitude">Высота, км</param>
        /// <param name="density">Плотность, кг/м^3</param>
        /// <param name="presure">Давление, !!!!!!!мм.рт.ст.!!!!!</param>
        /// <param name="temperature">Температура, К</param>
        public AtmosphereParams(double altitude, double temperature, double presure, double density)
        {
            Altitude = altitude;
            Density = density;
            Presure = 133.322*presure;
            Temperature = temperature;
            SoundVelocity = Math.Sqrt(401.8*temperature);
        }
    }
}
