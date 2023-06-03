using System.Collections.Generic;
using System.Linq;
namespace CalculationCore{
public class AtmosphereInterpolator{
private readonly List<AtmosphereParams> atmosphereParams = new List<AtmosphereParams>();
private readonly double[] temperature = new double[300001];
private readonly double[] density = new double[300001];
private readonly double[] presure = new double[300001];
private readonly double[] soundVelocity = new double[300001];

public AtmosphereInterpolator()
{
    InitializeAtmosphereParams();
    Interpolate();
}

public double GetTemperature(double altitude)
{
    if (altitude <= 0) return temperature[0];
    return altitude >= 300000 ? temperature[300000] : temperature[(int) altitude];
}

public double GetDensity(double altitude)
{
    double result;
    if (altitude <= 0) result = density[0];
    else result = altitude >= 300000 ? density[300000] : density[(int)altitude];
    return result;
}

public double GetPresure(double altitude)
{
    if (altitude <= 0) return presure[0];
    return altitude >= 300000 ? presure[300000] : presure[(int)altitude];
}

public double GetSoundVelocity(double altitude)
{
    if (altitude <= 0) return soundVelocity[0];
    return altitude >= 300000 ? soundVelocity[300000] : soundVelocity[(int)altitude];
}

private void Interpolate()
{
    for (var index = 0; index <= 300000; index++)
    {
        var currentParams = InterpolateParams(index/1000.0);
        temperature[index] = currentParams.Temperature;
        density[index] = currentParams.Density;
        presure[index] = currentParams.Presure;
        soundVelocity[index] = currentParams.SoundVelocity;
    }
}

private AtmosphereParams InterpolateParams(double altitude)
{
    if (altitude >= 300.0) return atmosphereParams.Last();
    if (altitude <= 0.0) return atmosphereParams.First();
    var higherParam = atmosphereParams.First(par => par.Altitude >= altitude);
    var lowerParam = atmosphereParams.ElementAt(atmosphereParams.IndexOf(higherParam)-1);
    return new AtmosphereParams(
        altitude,
        lowerParam.Temperature + (altitude - lowerParam.Altitude) * (higherParam.Temperature - lowerParam.Temperature) /
        (higherParam.Altitude - lowerParam.Altitude),
        lowerParam.PresureInTorr + (altitude - lowerParam.Altitude) * (higherParam.PresureInTorr - lowerParam.PresureInTorr) /
        (higherParam.Altitude - lowerParam.Altitude),
        lowerParam.Density + (altitude - lowerParam.Altitude) * (higherParam.Density - lowerParam.Density) /
        (higherParam.Altitude - lowerParam.Altitude)
        );
}

private void InitializeAtmosphereParams()
{
    atmosphereParams.Add(new AtmosphereParams(0.0, 288.15, 760.00, 1.2250));
    atmosphereParams.Add(new AtmosphereParams(0.5, 284.90, 715.96, 1.1672));
    atmosphereParams.Add(new AtmosphereParams(1.0, 282.65, 674.12, 1.1117));
    atmosphereParams.Add(new AtmosphereParams(1.5, 278.40, 634.30, 1.0582));
    atmosphereParams.Add(new AtmosphereParams(2.0, 275.14, 596.28, 1.0066));
    atmosphereParams.Add(new AtmosphereParams(2.5, 271.89, 560.24, 0.95706));
    atmosphereParams.Add(new AtmosphereParams(3.0, 268.64, 525.98, 0.90941));
    atmosphereParams.Add(new AtmosphereParams(3.5, 265.38, 493.35, 0.86345));
    atmosphereParams.Add(new AtmosphereParams(4.0, 262.13, 462.46, 0.81942));
    atmosphereParams.Add(new AtmosphereParams(4.5, 258.88, 433.15, 0.77714));
    atmosphereParams.Add(new AtmosphereParams(5.0, 252.38, 405.37, 0.73654));
    atmosphereParams.Add(new AtmosphereParams(5.5, 278.40, 379.04, 0.69758));
    atmosphereParams.Add(new AtmosphereParams(6.0, 249.13, 354.13, 0.66022));
    atmosphereParams.Add(new AtmosphereParams(6.5, 245.88, 330.54, 0.62441));
    atmosphereParams.Add(new AtmosphereParams(7.0, 242.63, 308.26, 0.59010));
    atmosphereParams.Add(new AtmosphereParams(7.5, 239.38, 287.20, 0.55725));
    atmosphereParams.Add(new AtmosphereParams(8.0, 236.14, 267.38, 0.52591));
    atmosphereParams.Add(new AtmosphereParams(8.5, 232.89, 248.62, 0.49585));
    atmosphereParams.Add(new AtmosphereParams(9.0, 229.64, 230.95, 0.46712));
    atmosphereParams.Add(new AtmosphereParams(9.5, 226.40, 214.36, 0.43977));
    atmosphereParams.Add(new AtmosphereParams(10, 223.15, 198.70, 0.41357));
    atmosphereParams.Add(new AtmosphereParams(11, 216.66, 170.19, 0.36485));
    atmosphereParams.Add(new AtmosphereParams(12, 216.66, 145.44, 0.31180));
    atmosphereParams.Add(new AtmosphereParams(13, 216.66, 124.30, 0.26648));
    atmosphereParams.Add(new AtmosphereParams(14, 216.66, 106.24, 0.22776));
    atmosphereParams.Add(new AtmosphereParams(15, 216.66, 90.810, 0.19467));
    atmosphereParams.Add(new AtmosphereParams(16, 216.66, 77.616, 0.16640));
    atmosphereParams.Add(new AtmosphereParams(17, 216.66, 66.350, 0.14224));
    atmosphereParams.Add(new AtmosphereParams(18, 216.66, 56.719, 0.12159));
    atmosphereParams.Add(new AtmosphereParams(19, 216.66, 48.489, 0.10395));
    atmosphereParams.Add(new AtmosphereParams(20, 216.66, 41.455, 0.088870));
    atmosphereParams.Add(new AtmosphereParams(22, 216.66, 30.305, 0.064966));
    atmosphereParams.Add(new AtmosphereParams(24, 216.66, 22.158, 0.047501));
    atmosphereParams.Add(new AtmosphereParams(26, 216.40, 16.219, 0.034336));
    atmosphereParams.Add(new AtmosphereParams(28, 224.87, 11.959, 0.024701));
    atmosphereParams.Add(new AtmosphereParams(30, 230.35, 8.8777, 0.017901));
    atmosphereParams.Add(new AtmosphereParams(35, 244.01, 4.3522, 8.2842e-3));
    atmosphereParams.Add(new AtmosphereParams(40, 257.66, 2.2191, 4.0003e-3));
    atmosphereParams.Add(new AtmosphereParams(45, 271.28, 1.1732, 2.0086e-3));
    atmosphereParams.Add(new AtmosphereParams(50, 274.00, 0.63441, 1.0754e-3));
    atmosphereParams.Add(new AtmosphereParams(60, 253.40, 0.18092, 3.3162e-4));
    atmosphereParams.Add(new AtmosphereParams(70, 219.15, 0.043761, 9.2747e-5));
    atmosphereParams.Add(new AtmosphereParams(80, 185.00, 0.0083564, 2.0979e-5));
    atmosphereParams.Add(new AtmosphereParams(90, 185.00, 0.0013834, 3.4733e-6));
    atmosphereParams.Add(new AtmosphereParams(95, 185.00, 5.6408e-4, 1.4170e-6));
    atmosphereParams.Add(new AtmosphereParams(100, 209.22, 2.4310e-4, 5.3993e-7));
    atmosphereParams.Add(new AtmosphereParams(105, 233.36, 1.1604e-4, 2.2900e-7));
    atmosphereParams.Add(new AtmosphereParams(110, 257.36, 5.8671e-5, 1.0583e-7));
    atmosphereParams.Add(new AtmosphereParams(115, 294.97, 3.2314e-5, 5.0674e-8));
    atmosphereParams.Add(new AtmosphereParams(120, 332.24, 1.9165e-5, 2.6586e-8));
    atmosphereParams.Add(new AtmosphereParams(125, 442.64, 1.2555e-5, 1.3025e-8));
    atmosphereParams.Add(new AtmosphereParams(130, 552.04, 9.0540e-6, 7.5055e-9));
    atmosphereParams.Add(new AtmosphereParams(135, 660.51, 6.9357e-6, 4.7873e-9));
    atmosphereParams.Add(new AtmosphereParams(140, 768.00, 5.5394e-6, 3.2766e-9));
    atmosphereParams.Add(new AtmosphereParams(145, 874.48, 4.5608e-6, 2.3605e-9));
    atmosphereParams.Add(new AtmosphereParams(150, 980.05, 3.8428e-6, 1.7682e-9));
    atmosphereParams.Add(new AtmosphereParams(155, 1068.1, 3.2937e-6, 1.3855e-9));
    atmosphereParams.Add(new AtmosphereParams(160, 1155.3, 2.8598e-6, 1.1081e-9));
    atmosphereParams.Add(new AtmosphereParams(165, 1165.6, 2.4996e-6, 9.5683e-10));
    atmosphereParams.Add(new AtmosphereParams(170, 1175.0, 2.1887e-6, 8.2787e-10));
    atmosphereParams.Add(new AtmosphereParams(175, 1184.2, 1.9200e-6, 7.1767e-10));
    atmosphereParams.Add(new AtmosphereParams(180, 1193.2, 1.6872e-6, 6.2332e-10));
    atmosphereParams.Add(new AtmosphereParams(185, 1202.0, 1.4850e-6, 5.4236e-10));
    atmosphereParams.Add(new AtmosphereParams(190, 1210.6, 1.3093e-6, 4.7276e-10));
    atmosphereParams.Add(new AtmosphereParams(195, 1218.9, 1.1562e-6, 4.1282e-10));
    atmosphereParams.Add(new AtmosphereParams(200, 1226.8, 1.0226e-6, 3.6109e-10));
    atmosphereParams.Add(new AtmosphereParams(205, 1236.0, 9.0586e-7, 3.1603e-10));
    atmosphereParams.Add(new AtmosphereParams(210, 1245.0, 8.0379e-7, 2.7709e-10));
    atmosphereParams.Add(new AtmosphereParams(215, 1253.7, 7.1435e-7, 2.4336e-10));
    atmosphereParams.Add(new AtmosphereParams(220, 1262.0, 6.3585e-7, 2.1412e-10));
    atmosphereParams.Add(new AtmosphereParams(225, 1269.9, 5.6684e-7, 1.8870e-10));
    atmosphereParams.Add(new AtmosphereParams(230, 1277.4, 5.0606e-7, 1.6656e-10));
    atmosphereParams.Add(new AtmosphereParams(235, 1284.4, 4.5245e-7, 1.4726e-10));
    atmosphereParams.Add(new AtmosphereParams(240, 1290.9, 4.0508e-7, 1.3039e-10));
    atmosphereParams.Add(new AtmosphereParams(245, 1297.0, 3.6317e-7, 1.1563e-10));
    atmosphereParams.Add(new AtmosphereParams(250, 1302.8, 3.2604e-7, 1.0270e-10));
    atmosphereParams.Add(new AtmosphereParams(255, 1309.5, 2.9311e-7, 9.1190e-11));
    atmosphereParams.Add(new AtmosphereParams(260, 1316.2, 2.6390e-7, 8.1111e-11));
    atmosphereParams.Add(new AtmosphereParams(265, 1322.6, 2.3794e-7, 7.2256e-11));
    atmosphereParams.Add(new AtmosphereParams(270, 1328.8, 2.1481e-7, 6.4465e-11));
    atmosphereParams.Add(new AtmosphereParams(275, 1334.6, 1.9421e-7, 5.7605e-11));
    atmosphereParams.Add(new AtmosphereParams(280, 1340.0, 1.7581e-7, 5.1548e-11));
    atmosphereParams.Add(new AtmosphereParams(290, 1349.5, 1.4465e-7, 4.1463e-11));
    atmosphereParams.Add(new AtmosphereParams(295, 1353.9, 1.3142e-7, 3.7253e-11));
    atmosphereParams.Add(new AtmosphereParams(300, 1358.0, 1.1956e-7, 3.3521e-11));	 
}}}
