using System;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

// 在正式路径刷新装配引用和重建标志，不通过桌面操作。
internal static class RebuildAssembly
{
    public static int Main(string[] args)
    {
        SldWorks sw = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            foreach (string input in args)
            {
                int e = 0, w = 0;
                string path = Path.GetFullPath(input);
                ModelDoc2 model = sw.OpenDoc6(path, (int)swDocumentTypes_e.swDocASSEMBLY,
                    (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref e, ref w);
                if (model == null) throw new IOException("Cannot open " + path);
                Console.WriteLine("open=" + Path.GetFileName(path) + "|errors=" + e + "|warnings=" + w);
                bool rebuilt = model.ForceRebuild3(false);
                bool saved = model.Save3((int)swSaveAsOptions_e.swSaveAsOptions_Silent, ref e, ref w);
                Console.WriteLine("rebuild=" + rebuilt + "|saved=" + saved + "|errors=" + e + "|warnings=" + w);
                if (!saved || e != 0) throw new IOException("Cannot save " + path);
                sw.CloseDoc(model.GetTitle());
            }
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
        finally
        {
            if (sw != null)
            {
                try { sw.ExitApp(); } catch { }
                Marshal.FinalReleaseComObject(sw);
            }
        }
    }
}
