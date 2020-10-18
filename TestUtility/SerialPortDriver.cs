using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace TestUtility
{
    public class SerialPortDriver : SerialPort
    {
        
        public SerialPortDriver(String Port ,int baud, Parity p, int databits, StopBits s ) :base(Port,baud,p,databits,s)
        {
            
        }
        public void WriteWithDelay(byte[] buf, int offset, int count, int delay)
        {
            for(int i = offset; i < offset + count; i++)
            {
                byte[] data = new byte[1];
                data[0] = buf[i];
                Write(data, 0, 1);
                Thread.Sleep(delay);
            }
        }
    }
}
