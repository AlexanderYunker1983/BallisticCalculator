using System.Windows;
using BallisticCalculator.Wpf.ViewModels;

namespace BallisticCalculator.Wpf
{
    public partial class MainWindow : Window
    {
        private readonly MainWindowViewModel _viewModel = new();

        public MainWindow()
        {
            InitializeComponent();
            DataContext = _viewModel;
        }

        private void InitialParamsButton_Click(object sender, RoutedEventArgs e)
        {
            var window = new InitialParamsWindow
            {
                Owner = this,
                DataContext = _viewModel
            };

            window.ShowDialog();
        }
    }
}

