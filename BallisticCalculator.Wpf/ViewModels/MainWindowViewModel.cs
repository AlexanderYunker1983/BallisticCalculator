using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CalculationCore;
using OxyPlot;
using OxyPlot.Axes;
using OxyPlot.Series;
using OxyPlot.Annotations;

namespace BallisticCalculator.Wpf.ViewModels;

public record ChartPoint(double X, double Y);

public partial class MainWindowViewModel : ObservableObject
{
    private readonly CalculationCore.CalculationCore _calculationCore = new();
    private InitialParams _initialParams;

    // Параметры ракеты (редактируемые пользователем)
    [ObservableProperty] private double _massGo      = 3000;
    [ObservableProperty] private double _mass3       = 7700;
    [ObservableProperty] private double _mass2       = 29920;
    [ObservableProperty] private double _mass1       = 70480;

    [ObservableProperty] private double _massFuel3   = 7346;
    [ObservableProperty] private double _massFuel2   = 26360;
    [ObservableProperty] private double _massFuel1   = 60380;

    [ObservableProperty] private double _thrust1     = 1470000;
    [ObservableProperty] private double _thrust2     = 392000;
    [ObservableProperty] private double _thrust3     = 110000;

    [ObservableProperty] private double _isp1        = 3297.5;
    [ObservableProperty] private double _isp2        = 3498;
    [ObservableProperty] private double _isp3        = 3295.6;

    [ObservableProperty] private double _phi0Time    = 40.0;

    [ObservableProperty]
    private string _phi1Time = "310";

    [ObservableProperty]
    private string _phi1Degrees = "14,97";

    [ObservableProperty]
    private string _velocityInfo = string.Empty;

    [ObservableProperty]
    private string _altitudeInfo = string.Empty;

    [ObservableProperty]
    private bool _isBusy;

    public bool IsNotBusy => !IsBusy;

    // WPF-графики (OxyPlot)
    [ObservableProperty]
    private PlotModel _velocityPlotModel = new() { Title = "Скорость" };

    [ObservableProperty]
    private PlotModel _altitudePlotModel = new() { Title = "Высота" };

    [ObservableProperty]
    private PlotModel _alphaPlotModel = new() { Title = "Угол атаки" };

    [ObservableProperty]
    private PlotModel _phiPlotModel = new() { Title = "Программа" };

    [ObservableProperty]
    private PlotModel _overloadPlotModel = new() { Title = "Перегрузка" };

    [ObservableProperty]
    private PlotModel _orthodromicRangePlotModel = new() { Title = "Ортодромная дальность" };

    [ObservableProperty]
    private PlotModel _heightProfilePlotModel = new() { Title = "Профиль высоты" };

    public ObservableCollection<ChartPoint> VelocitySeries { get; } = new();
    public ObservableCollection<ChartPoint> AltitudeSeries { get; } = new();
    public ObservableCollection<ChartPoint> AlphaSeries { get; } = new();
    public ObservableCollection<ChartPoint> PhiSeries { get; } = new();
    public ObservableCollection<ChartPoint> OverloadSeries { get; } = new();

    public MainWindowViewModel()
    {
        InitializePlotModels();
    }

    // Асинхронная команда запуска расчёта (RunCommand / RunCommand.CancelCommand)
    [RelayCommand(IncludeCancelCommand = true)]
    private async Task Run(CancellationToken cancellationToken)
    {
        IsBusy = true;

        try
        {
            // Парсинг с поддержкой запятой как десятичного разделителя
            var culture = CultureInfo.CurrentCulture;

            if (!double.TryParse(Phi1Time, NumberStyles.Float, culture, out var phi1Time))
            {
                // Некорректный ввод – просто выходим, можно добавить валидацию при желании
                return;
            }

            if (!double.TryParse(Phi1Degrees, NumberStyles.Float, culture, out var phi1Deg))
            {
                return;
            }

            _initialParams = new InitialParams
            {
                MassGo    = MassGo,
                Mass3     = Mass3,
                Mass2     = Mass2,
                Mass1     = Mass1,
                MassFuel3 = MassFuel3,
                MassFuel2 = MassFuel2,
                MassFuel1 = MassFuel1,
                Thrust1   = Thrust1,
                Thrust2   = Thrust2,
                Thrust3   = Thrust3,
                Isp1      = Isp1,
                Isp2      = Isp2,
                Isp3      = Isp3,
                Phi0Time  = Phi0Time,
                Phi1Time  = phi1Time,
                Phi1      = phi1Deg / 180.0 * Math.PI
            };

            _initialParams.Initialize();
            _calculationCore.SetInitParams(_initialParams, 0.01);

            var result = await _calculationCore.RunAsync(cancellationToken).ConfigureAwait(false);

            // Возврат на UI-поток
            await System.Windows.Application.Current.Dispatcher.InvokeAsync(() =>
            {
                ApplyCalculationResult(result);
            });
        }
        catch (OperationCanceledException)
        {
            // Отмена пользователем — просто выходим без ошибок
        }
        finally
        {
            IsBusy = false;
        }
    }

    private void ApplyCalculationResult(CalculationResult result)
    {
        // Обновляем модели графиков (OxyPlot)
        UpdatePlots(new System.Collections.Generic.List<CalculationVector>(result.Trajectory));

        var last = result.Trajectory[^1];
        var velocityOrbit = Math.Sqrt(
            CalculationVector.G0 *
            Math.Pow(CalculationVector.RadiusOfEarth, 2) /
            last.Radius);

        // Округление до 3 знаков после запятой с учётом локали
        var uiCulture = CultureInfo.CurrentCulture;

        VelocityInfo = string.Format(
            uiCulture,
            "{0} -> {1}",
            Math.Round(last.Velocity, 3),
            Math.Round(velocityOrbit, 3));

        AltitudeInfo = Math.Round(
                (last.Radius - CalculationVector.RadiusOfEarth) / 1000.0,
                3)
            .ToString("F3", uiCulture);

        // Обновляем связанные с вводом значения (округление до 3 знаков)
        Phi1Time = Math.Round(result.Phi1Time, 3).ToString("F3", uiCulture);
        Phi1Degrees = Math.Round(result.Phi1Degrees, 3).ToString("F3", uiCulture);
    }

    partial void OnIsBusyChanged(bool value)
    {
        OnPropertyChanged(nameof(IsNotBusy));
    }

    private void InitializePlotModels()
    {
        ConfigurePlotModel(VelocityPlotModel, "t, c", "V, м/с");
        ConfigurePlotModel(AltitudePlotModel, "t, c", "H, км");
        ConfigurePlotModel(AlphaPlotModel, "t, c", "α, град");
        ConfigurePlotModel(PhiPlotModel, "t, c", "φ, град");
        ConfigurePlotModel(OverloadPlotModel, "t, c", "n, g");
        ConfigurePlotModel(OrthodromicRangePlotModel, "t, c", "S, км");
        ConfigurePlotModel(HeightProfilePlotModel, "S, км", "H, км");
    }

    private static void ConfigurePlotModel(PlotModel model, string xTitle, string yTitle)
    {
        model.Axes.Clear();

        var majorGridColor = OxyColor.FromRgb(200, 200, 200);   // более светлый серый для подписанных делений
        var minorGridColor = OxyColor.FromRgb(240, 240, 240);   // ещё более светлый серый для рисок без подписей

        var xAxis = new LinearAxis
        {
            Position = AxisPosition.Bottom,
            Title = xTitle,
            MajorGridlineStyle = LineStyle.Solid,
            MajorGridlineColor = majorGridColor,
            MinorGridlineStyle = LineStyle.Solid,
            MinorGridlineColor = minorGridColor,
            AxislineStyle = LineStyle.Solid,
            AxislineColor = OxyColors.Black
        };

        var yAxis = new LinearAxis
        {
            Position = AxisPosition.Left,
            Title = yTitle,
            MajorGridlineStyle = LineStyle.Solid,
            MajorGridlineColor = majorGridColor,
            MinorGridlineStyle = LineStyle.Solid,
            MinorGridlineColor = minorGridColor,
            AxislineStyle = LineStyle.Solid,
            AxislineColor = OxyColors.Black
        };

        model.Axes.Add(xAxis);
        model.Axes.Add(yAxis);
    }

    private void UpdatePlots(System.Collections.Generic.List<CalculationVector> vectors)
    {
        // Для плавных графиков:
        // - не округляем время (используем CurrentTime как есть),
        // - включаем сглаживающую интерполяцию Catmull-Rom.
        var velSeries = new LineSeries
        {
            Title = "Скорость",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };
        var altSeries = new LineSeries
        {
            Title = "Высота",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };
        var alphaSeries = new LineSeries
        {
            Title = "Угол атаки",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };
        var phiSeries = new LineSeries
        {
            Title = "Программа",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };
        var overloadSeries = new LineSeries
        {
            Title = "Перегрузка",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };

        var orthodromicSeries = new LineSeries
        {
            Title = "Ортодромная дальность",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };

        var heightProfileSeries = new LineSeries
        {
            Title = "Профиль высоты",
            InterpolationAlgorithm = InterpolationAlgorithms.CatmullRomSpline
        };

        foreach (var v in vectors)
        {
            var t = v.CurrentTime;
            var altitudeKm = (v.Radius - CalculationVector.RadiusOfEarth) / 1000.0;

            alphaSeries.Points.Add(new DataPoint(t, v.Alpha * 180 / Math.PI));
            phiSeries.Points.Add(new DataPoint(t, v.Phi * 180 / Math.PI));
            altSeries.Points.Add(new DataPoint(t, altitudeKm));
            velSeries.Points.Add(new DataPoint(t, v.Velocity));
            overloadSeries.Points.Add(new DataPoint(t, v.Acceleration / 9.80665 + 1.0));
            // ортодромная дальность: дуга по поверхности Земли от точки старта
            var rangeKm = CalculationVector.RadiusOfEarth * v.Hi / 1000.0;
            orthodromicSeries.Points.Add(new DataPoint(t, rangeKm));
            // профиль высоты: высота как функция ортодромной дальности
            heightProfileSeries.Points.Add(new DataPoint(rangeKm, altitudeKm));
        }

        // Обновляем серии
        VelocityPlotModel.Series.Clear();
        VelocityPlotModel.Series.Add(velSeries);

        AltitudePlotModel.Series.Clear();
        AltitudePlotModel.Series.Add(altSeries);

        AlphaPlotModel.Series.Clear();
        AlphaPlotModel.Series.Add(alphaSeries);

        PhiPlotModel.Series.Clear();
        PhiPlotModel.Series.Add(phiSeries);

        OverloadPlotModel.Series.Clear();
        OverloadPlotModel.Series.Add(overloadSeries);

        OrthodromicRangePlotModel.Series.Clear();
        OrthodromicRangePlotModel.Series.Add(orthodromicSeries);

        HeightProfilePlotModel.Series.Clear();
        HeightProfilePlotModel.Series.Add(heightProfileSeries);

        // Обновляем аннотации с моментами разделения ступеней
        AddStageSeparationAnnotations(VelocityPlotModel);
        AddStageSeparationAnnotations(AltitudePlotModel);
        AddStageSeparationAnnotations(AlphaPlotModel);
        AddStageSeparationAnnotations(PhiPlotModel);
        AddStageSeparationAnnotations(OverloadPlotModel);
        AddStageSeparationAnnotations(OrthodromicRangePlotModel);
        // Для профиля высоты вертикальные линии отделения ступеней не так информативны,
        // поэтому здесь их не добавляем.

        VelocityPlotModel.InvalidatePlot(true);
        AltitudePlotModel.InvalidatePlot(true);
        AlphaPlotModel.InvalidatePlot(true);
        PhiPlotModel.InvalidatePlot(true);
        OverloadPlotModel.InvalidatePlot(true);
        OrthodromicRangePlotModel.InvalidatePlot(true);
        HeightProfilePlotModel.InvalidatePlot(true);
    }

    private void AddStageSeparationAnnotations(PlotModel model)
    {
        model.Annotations.Clear();

        if (_initialParams.Time1 <= 0 || _initialParams.TimeSumm2 <= 0 || _initialParams.TimeSumm3 <= 0)
        {
            return;
        }

        var times = new[] { _initialParams.Time1, _initialParams.TimeSumm2, _initialParams.TimeSumm3 };

        foreach (var t in times)
        {
            model.Annotations.Add(new LineAnnotation
            {
                Type = LineAnnotationType.Vertical,
                X = t,
                Color = OxyColors.Red,
                LineStyle = LineStyle.Solid,
                StrokeThickness = 1.5
            });
        }
    }
}

