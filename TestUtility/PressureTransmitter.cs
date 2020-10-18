using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel;
using System.ComponentModel.Design;
using System.Diagnostics;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Timers;

namespace TestUtility
{
    class PressureTransmitterManager
    {
        
        SerialPortDriver Port;
        public ConcurrentQueue<String> CommandQ;
        public ConcurrentQueue<String> ResponseQ;

        byte[] CommandFrame_1 = { 0x01, 0x03, 0x00, 0x11, 0x00, 0x01, 0xD4, 0x0F };
        byte[] CommandFrame_2 = { 0x02, 0x03, 0x00, 0x11, 0x00, 0x01, 0xD4, 0x3C };

        ConcurrentQueue<PressureTransmitter> Transmitters;

        public ConcurrentQueue<String> LogQ;
        Timer TickTimer;
        public PressureTransmitterManager(string com, int baud, Parity p, int databits, StopBits s , int transmitterCount)
        {
            Port = new SerialPortDriver(com, baud, p, databits, s);
            Port.ReadTimeout = 3000;
            Port.Open();
            CommandQ = new ConcurrentQueue<string>();
            ResponseQ = new ConcurrentQueue<string>();


           
            TickTimer = new Timer(1000);
            TickTimer.Elapsed += TickTimer_Elapsed;
            TickTimer.AutoReset = false;
            
           

            Transmitters = new ConcurrentQueue<PressureTransmitter>();
            for(int i = 1; i <= transmitterCount;i++)
            {
                Transmitters.Enqueue(new PressureTransmitter(i));

            }
           
            TickTimer.Start();
        }

        private void TickTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            TickTimer.Stop();

            string command = string.Empty;
            try
            {
                if (CommandQ.TryDequeue(out command))
                {



                    if (command.Contains("FLRD"))
                    {
                        //LogQ.Enqueue(command);

                        Port.DiscardInBuffer();
                        foreach (PressureTransmitter p in Transmitters)
                        {
                            p.FillPressure = 0;
                            byte[] res = new byte[10];
                            do
                            {
                                Port.DiscardInBuffer();
                                for (int i = 0; i < 7; i++)
                                    res[i] = 0;

                                Port.WriteWithDelay(p.ID == 1 ? CommandFrame_1 : CommandFrame_2, 0, 8, 10);
                                for (int i = 0; i < 7; i++)
                                {
                                    res[i] = (byte)Port.ReadByte();

                                }



                            } while ((res[0] != p.ID) || (res[4] == 0));

                            p.FillPressure = res[4];

                        }



                        //  ResponseQ.Enqueue("RDFL");
                    }

                    else if (command.Contains("TSRD"))
                    {
                        //LogQ.Enqueue(command);


                        foreach (PressureTransmitter p in Transmitters)
                        {
                            p.TestPressure = 0;
                            byte[] res = new byte[10];
                            do
                            {
                                Port.DiscardInBuffer();
                                for (int i = 0; i < 7; i++)
                                    res[i] = 0;

                                Port.WriteWithDelay(p.ID == 1 ? CommandFrame_1 : CommandFrame_2, 0, 8, 10);
                                for (int i = 0; i < 7; i++)
                                {
                                    res[i] = (byte)Port.ReadByte();

                                }


                            } while (res[0] != p.ID);

                            p.TestPressure = res[4];
                        }


                        // ResponseQ.Enqueue("RDTS");

                    }

                    else if (command.Contains("RSRD"))
                    {
                        //LogQ.Enqueue(command);
                        string result = "";

                        foreach (PressureTransmitter p in Transmitters)
                        {
                            result += p.GetResult() ? "P" : "F";
                            p.FillPressure /= 10;
                            p.TestPressure /= 10;
                            LogQ.Enqueue("STN_" + p.ID + "- Fill: " + p.FillPressure.ToString("0.00") + " Test: " + p.TestPressure.ToString("0.00")
                                + " Delta: " + (p.FillPressure - p.TestPressure).ToString("0.00") + "\tResult : " + result);
                            
                        }

                        ResponseQ.Enqueue(result);


                    }

                }





                else
                {
                    foreach (PressureTransmitter p in Transmitters)
                    {
                        Port.WriteWithDelay(p.ID == 1 ? CommandFrame_1 : CommandFrame_2, 0, 8, 10);
                        for (int i = 0; i < 7; i++)
                        {
                            Port.ReadByte();

                        }
                    }
                }

            }
            catch (Exception ex)
            {
                LogQ.Enqueue(ex.Message);
                if (Port.IsOpen)
                    Port.Close();
                Port.Open();
            }

            TickTimer.Start();
        }

       
    }

    class PressureTransmitter
    {
        public int ID { get; set; }
        public double FillPressure { get; set; }
        public double TestPressure { get; set; }

        public double MinDiff { get; set; }
        public double MaxDiff { get; set; }

       
        public PressureTransmitter(int id)
        {
            ID = id;
            FillPressure = TestPressure = MinDiff = MaxDiff = 0;
            MinDiff = -1.0;
            MaxDiff = +1.0;
        }

        public bool GetResult()
        {
            if (FillPressure < 30) return false;
            double diff = FillPressure - TestPressure;
            
            if ((diff >= MinDiff) && (diff <= MaxDiff))
                return true;
            else return false;
            
        }
    }
}
