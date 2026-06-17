using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Runtime.InteropServices;

namespace Blackwater.Editor;

/// <summary>
/// Interaction logic for MainWindow.xaml
/// </summary>
public partial class MainWindow : Window
{
    [LibraryImport("Blackwater.Core.dll", EntryPoint = "InitializeEngine")]
    private static partial int InitializeEngine();
    
    [LibraryImport("Blackwater.Core.dll",  EntryPoint = "ShutdownEngine")]
    private static partial int ShutdownEngine();
    
    
    public MainWindow()
    {
        InitializeComponent();
        
        int status =  InitializeEngine();

        if (status == 1)
        {
            Title = "Blackwater Editor | Engine Core Online";
        }
        else
        {
            MessageBox.Show(
                "Failed to initialize Blackwater C++ Core Engine.", 
                "Critical Engine Error",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            Application.Current.Shutdown();
        }
    }
    
    protected override void OnClosed(EventArgs e)
    {
        ShutdownEngine();
        base.OnClosed(e);
    }
}