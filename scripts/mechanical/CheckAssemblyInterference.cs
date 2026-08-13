using System;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class CheckAssemblyInterference
{
    public static int Main(string[] args)
    {
        if (args.Length < 1) return 2;
        SldWorks sw = null;
        ModelDoc2 model = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            int errors = 0, warnings = 0;
            model = sw.OpenDoc6(Path.GetFullPath(args[0]),
                (int)swDocumentTypes_e.swDocASSEMBLY,
                (int)swOpenDocOptions_e.swOpenDocOptions_Silent,
                "", ref errors, ref warnings);
            Console.WriteLine(string.Format("open={0}|errors={1}|warnings={2}", model != null, errors, warnings));
            if (model == null) return 3;

            AssemblyDoc assembly = (AssemblyDoc)model;
            InterferenceDetectionMgr manager = assembly.InterferenceDetectionManager;
            manager.TreatCoincidenceAsInterference = false;
            manager.IgnoreHiddenBodies = false;
            manager.IncludeMultibodyPartInterferences = true;
            int count = manager.GetInterferenceCount();
            Console.WriteLine("interferenceCount=" + count);
            object[] items = manager.GetInterferences() as object[];
            if (items != null)
            {
                for (int i = 0; i < items.Length; i++)
                {
                    Interference interference = (Interference)items[i];
                    object[] components = interference.Components as object[];
                    string names = "";
                    if (components != null)
                    {
                        for (int j = 0; j < components.Length; j++)
                        {
                            Component2 component = (Component2)components[j];
                            if (j > 0) names += " + ";
                            names += component.Name2;
                        }
                    }
                    Console.WriteLine(string.Format("interference={0}|volume_mm3={1:0.######}|{2}",
                        i, interference.Volume * 1.0e9, names));
                }
            }
            manager.Done();
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 1;
        }
        finally
        {
            if (model != null && sw != null) try { sw.CloseDoc(model.GetTitle()); } catch { }
            if (sw != null)
            {
                try { sw.ExitApp(); } catch { }
                try { Marshal.FinalReleaseComObject(sw); } catch { }
            }
        }
    }
}
