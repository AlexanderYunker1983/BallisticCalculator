using System.Windows;

namespace BallisticCalculator.Wpf
{
    public partial class InitialParamsWindow : Window
    {
        public InitialParamsWindow()
        {
            InitializeComponent();
        }

        private void OkButton_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = true;
            Close();
        }
    }
}

