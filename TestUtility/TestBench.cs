using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace TestUtility
{
    public class TestBench
    {
        BackgroundWorker Worker;

        SerialPortDriver Port;

        public ConcurrentQueue<String> CommandQ;
        public ConcurrentQueue<String> ResponseQ;

        public ConcurrentQueue<String> LogQ;
        public TestBench(String port, int baud, Parity p, int databits , StopBits s)
        {
            Port = new SerialPortDriver(port, baud, p, databits, s);
            Port.Open();
            CommandQ = new ConcurrentQueue<string>();
            ResponseQ = new ConcurrentQueue<string>();

            Worker = new BackgroundWorker();
            Worker.WorkerSupportsCancellation = true;
            Worker.DoWork += Worker_DoWork;
            Worker.RunWorkerAsync();



        }

        private void Worker_DoWork(object sender, DoWorkEventArgs e)
        {
            string data = string.Empty;
            while(true)
            {
                if(Port.BytesToRead > 0)
                {
                    data = Port.ReadLine();
                    CommandQ.Enqueue(String.Copy(data));
                    LogQ.Enqueue(String.Copy(data));

                }
                string res = string.Empty;
                if(ResponseQ.TryDequeue(out res))
                {
                    Port.Write(res);
                }
               
            }
        }
    }
}
