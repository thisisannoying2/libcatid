/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 2.0.2
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */


using System;
using System.Runtime.InteropServices;

public class Sphynx {


    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool SetDllDirectory(string lpPathName);

    public static void AssemblyResolveHook()
	{
#if (DEBUG)
        SetDllDirectory(string.Format("sphynx_d{0}", (IntPtr.Size == 4) ? "32" : "64"));
#else
		SetDllDirectory(string.Format("sphynx_r{0}", (IntPtr.Size == 4) ? "32" : "64"));
#endif
    }

    [StructLayoutAttribute(LayoutKind.Sequential,Pack=1)]
    private struct RawIncomingMessage
    {
        public IntPtr data;
        public UInt32 bytes;
        public UInt32 stream;
        public UInt32 send_time;
        public byte huge_fragment; // true = part of a huge transfer, last fragment will have bytes = 0
    }

    public struct IncomingMessage
    {
        public byte[] data;
        public bool is_huge;
        public uint send_time;
        public uint stream;
    }

    public static unsafe IncomingMessage[] GetMessages(IntPtr msgs, int count)
    {
        IncomingMessage[] array = new IncomingMessage[count];

        RawIncomingMessage* ptr = (RawIncomingMessage*)msgs;

        for (int ii = 0; ii < count; ++ii)
        {
            array[ii].data = new byte[ptr[ii].bytes];

            Marshal.Copy(ptr[ii].data, array[ii].data, 0, (int)ptr[ii].bytes);

            array[ii].is_huge = ptr[ii].huge_fragment != 0;
            array[ii].send_time = ptr[ii].send_time;
            array[ii].stream= ptr[ii].stream;
        }

        return array;
    }


}
