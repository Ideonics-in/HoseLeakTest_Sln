using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.IO.Ports;
using System.ComponentModel;
using System.Security.RightsManagement;
using System.Threading;
using System.Reflection;
using System.Collections.Concurrent;
using System.Configuration;

namespace TestUtility
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        
        System.Collections.Concurrent.ConcurrentQueue<String> TestBenchLogQ;
        System.Collections.Concurrent.ConcurrentQueue<String> TransmitterLogQ;

        BackgroundWorker Communicator = new BackgroundWorker();
        BackgroundWorker Logger = new BackgroundWorker();
        BackgroundWorker TestBench = new BackgroundWorker();


        TestBench HoseLeakTestBench;
        PressureTransmitterManager pressureTransmitterManager;
        public MainWindow()
        {
            try
            {
                InitializeComponent();

                TestBenchLogQ = new System.Collections.Concurrent.ConcurrentQueue<string>();
                TransmitterLogQ = new System.Collections.Concurrent.ConcurrentQueue<string>();

                Logger.DoWork += Logger_DoWork;
                Logger.WorkerSupportsCancellation = true;
                Logger.RunWorkerAsync();


                Communicator.DoWork += Communicator_DoWork;
                Communicator.WorkerSupportsCancellation = true;
                

                String testbenchport = ConfigurationManager.AppSettings.Get("TestBenchPort");

                String testPressure = ConfigurationManager.AppSettings.Get("TestPressure");

                Convert.ToDouble(testPressure);

                HoseLeakTestBench = new TestBench(testbenchport, 9600, Parity.None, 8, StopBits.One);
                HoseLeakTestBench.LogQ = TestBenchLogQ;
                String transmitterport = ConfigurationManager.AppSettings.Get("PressureTransmitterPort"); 
                pressureTransmitterManager = new PressureTransmitterManager(transmitterport,9600, Parity.None, 8, StopBits.One, 1);
                pressureTransmitterManager.LogQ = TransmitterLogQ;

                Communicator.RunWorkerAsync();

            }

            catch (Exception e)
            {
                TestBenchLogQ.Enqueue(e.Message);
            }

        }

      
        private void Logger_DoWork(object sender, DoWorkEventArgs e)
        {
            while(true)
            {
                if(TestBenchLogQ.Count > 0 )
                {
                    this.Dispatcher.BeginInvoke((Action)(() =>
                    {
                        String log = TestBenchLog.Text;
                        string newLog = string.Empty;
                        if(TestBenchLogQ.TryDequeue(out newLog))
                             TestBenchLog.Text = newLog + log;
                    }));
                }

                if (TransmitterLogQ.Count > 0)
                {
                    this.Dispatcher.BeginInvoke((Action)(() =>
                    {
                        String log = DataLog.Text;
                        string newLog = string.Empty;
                        if (TransmitterLogQ.TryDequeue(out newLog))
                            DataLog.Text = DateTime.Now.ToString("dd-MM-yyyy HH:mm:ss") + "\t"+ newLog + Environment.NewLine + log;
                    }));
                }
            }
        }

        private void Communicator_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                while (true)
                {
                    if (Communicator.CancellationPending)
                        return;


                    if (HoseLeakTestBench == null) continue;
                    string data = string.Empty;
                    if (HoseLeakTestBench.CommandQ.TryDequeue(out data))
                    {
                       pressureTransmitterManager.CommandQ.Enqueue(String.Copy(data));
                    }
                    data = string.Empty;
                    if (pressureTransmitterManager == null) continue;
                    if (pressureTransmitterManager.ResponseQ.TryDequeue(out data))
                    {
                        if (data == "P")
                            HoseLeakTestBench.ResponseQ.Enqueue("Pass");
                        else if (data == "F")
                            HoseLeakTestBench.ResponseQ.Enqueue("Fail");
                    }
                }
                

                
               
                

            }
            catch (Exception ex)
            {
                TestBenchLogQ.Enqueue(ex.Message);
               
            }

       
        }

      

        private void Window_Closing(object sender, CancelEventArgs e)
        {
            Communicator.CancelAsync();
        }
    }


   
}
