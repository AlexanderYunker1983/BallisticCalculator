using System.Collections.Generic;
using System.Linq;
using CalculationCore;
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace BallisticCalculator
{
    public partial class Form1 : Form
    {
        private readonly CalculationCore.CalculationCore calculationCore = new CalculationCore.CalculationCore();
        private InitialParams initialParams;

        public Form1()
        {
            InitializeComponent();
            calculationCore.WorkCompleted += OnWorkComplited;
        }

        private void OnWorkComplited(List<CalculationVector> obj, double time, double phi)
        {
            chart1.Series[0].Points.Clear();
            chart2.Series[0].Points.Clear();
            chart3.Series[0].Points.Clear();
            chart4.Series[0].Points.Clear();

            var indexer = 0;
            const int counter = 100;
            foreach (var calculationVector in obj)
            {
                indexer++;
                if (indexer != 1)
                {
                    if (indexer >= counter)
                    {
                        indexer = 0;
                    }
                    continue;
                }
                chart3.Series[0].Points.AddXY(Math.Round(calculationVector.CurrentTime), calculationVector.Alpha * 180 / 3.1415);
                //chart1.Series[0].Points.AddXY(calculationVector.CurrentTime, calculationVector.Tetta*180/3.1415);
                chart4.Series[0].Points.AddXY(Math.Round(calculationVector.CurrentTime), calculationVector.Phi * 180 / 3.1415);
                chart2.Series[0].Points.AddXY(Math.Round(calculationVector.CurrentTime), (calculationVector.Radius - CalculationVector.RadiusOfEarth) / 1000.0);
                chart1.Series[0].Points.AddXY(Math.Round(calculationVector.CurrentTime), calculationVector.Velocity);
                chart5.Series[0].Points.AddXY(Math.Round(calculationVector.CurrentTime), calculationVector.Acceleration / 9.80665 + 1.0);
                //chart1.Series[0].Points.AddXY(calculationVector.CurrentTime, calculationVector.Acceleration);
                //chart1.Series[0].Points.AddXY(calculationVector.CurrentTime, calculationVector.Massa);
                
            }
            label2.Text = string.Format("{0} -> {1}",obj.Last().Velocity.ToString(),Math.Sqrt(CalculationCore.CalculationVector.G0*Math.Pow(CalculationCore.CalculationVector.RadiusOfEarth,2)/(obj.Last().Radius)));
            label1.Text = ((obj.Last().Radius - CalculationVector.RadiusOfEarth)/1000.0).ToString();
            textBox1.Text = time.ToString();
            textBox2.Text = phi.ToString();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            initialParams = new InitialParams();
            initialParams.MassGo = 3000;
            initialParams.Mass3 = 7700;
            initialParams.Mass2 = 29920;
            initialParams.Mass1 = 70480;
            initialParams.MassFuel3 = 7346;
            initialParams.MassFuel2 = 26360;
            initialParams.MassFuel1 = 60380;
            initialParams.Thrust1 = 1470000;
            initialParams.Thrust2 = 392000;
            initialParams.Thrust3 = 110000;
            initialParams.Isp1 = 3297.5;
            initialParams.Isp2 = 3498;
            initialParams.Isp3 = 3295.6;
            initialParams.Phi0Time = 40.0;
            initialParams.Phi1Time = Convert.ToDouble(textBox1.Text);
            initialParams.Phi1 = Convert.ToDouble(textBox2.Text)/180.0*Math.PI;
            initialParams.Initialize();
            calculationCore.SetInitParams(initialParams,0.01);
            calculationCore.RunRungeKutta();
        }

        private void chart1_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            var plotter = sender as System.Windows.Forms.DataVisualization.Charting.Chart;
            using (MemoryStream ms = new MemoryStream())
            {
                plotter.SaveImage(ms, System.Drawing.Imaging.ImageFormat.Bmp);
                Bitmap bm = new Bitmap(ms);
                Clipboard.SetImage(bm);
            }
        }
    }
}
