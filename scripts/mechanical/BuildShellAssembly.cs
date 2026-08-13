using System;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class BuildShellAssembly
{
    private static void Save(ModelDoc2 model, string path)
    {
        int errors = 0, warnings = 0;
        bool ok = model.Extension.SaveAs(path,
            (int)swSaveAsVersion_e.swSaveAsCurrentVersion,
            (int)swSaveAsOptions_e.swSaveAsOptions_Silent,
            null, ref errors, ref warnings);
        Console.WriteLine(string.Format("save={0}|ok={1}|errors={2}|warnings={3}", path, ok, errors, warnings));
        if (!ok) throw new IOException("Save failed: " + errors);
    }

    private static MathTransform Translation(SldWorks sw, double z)
    {
        double[] values =
        {
            1,0,0, 0,1,0, 0,0,1,
            0,0,z,
            1,0,0,0
        };
        MathUtility utility = (MathUtility)sw.GetMathUtility();
        return (MathTransform)utility.CreateTransform(values);
    }

    public static int Main(string[] args)
    {
        if (args.Length < 3) return 2;
        string lowerPath = Path.GetFullPath(args[0]);
        string upperPath = Path.GetFullPath(args[1]);
        string outputDir = Path.GetFullPath(args[2]);
        string output = Path.Combine(outputDir, "遥控器外壳装配体.SLDASM");
        string preview = Path.Combine(outputDir, "遥控器外壳装配体_预览.png");
        string template = @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_assembly.asmdot";

        SldWorks sw = null;
        ModelDoc2 lowerDoc = null, upperDoc = null, model = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            sw.SetUserPreferenceStringValue((int)swUserPreferenceStringValue_e.swDefaultTemplateAssembly, template);
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swAlwaysUseDefaultTemplates, true);
            int e = 0, w = 0;
            lowerDoc = sw.OpenDoc6(lowerPath, (int)swDocumentTypes_e.swDocPART, (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref e, ref w);
            upperDoc = sw.OpenDoc6(upperPath, (int)swDocumentTypes_e.swDocPART, (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref e, ref w);
            if (lowerDoc == null || upperDoc == null) throw new InvalidOperationException("Cannot preload shell parts.");

            model = (ModelDoc2)sw.NewDocument(template, 0, 0, 0);
            if (model == null) throw new InvalidOperationException("Cannot create shell assembly.");
            AssemblyDoc assembly = (AssemblyDoc)model;
            Component2 lower = assembly.AddComponent5(lowerPath, 0, "", false, "", 0, 0, 0);
            Component2 upper = assembly.AddComponent5(upperPath, 0, "", false, "", 0, 0, 0);
            if (lower == null || upper == null) throw new InvalidOperationException("Cannot insert shell parts.");

            lower.Transform2 = Translation(sw, 0);
            upper.Transform2 = Translation(sw, 0.017650);
            model.ClearSelection2(true);
            lower.Select4(false, null, false);
            assembly.FixComponent();
            model.ClearSelection2(true);
            upper.Select4(false, null, false);
            assembly.FixComponent();
            model.ClearSelection2(true);
            model.ForceRebuild3(false);

            Save(model, output);
            model.ShowNamedView2("*Isometric", (int)swStandardViews_e.swIsometricView);
            model.ViewZoomtofit2();
            Save(model, preview);

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
            if (upperDoc != null && sw != null) try { sw.CloseDoc(upperDoc.GetTitle()); } catch { }
            if (lowerDoc != null && sw != null) try { sw.CloseDoc(lowerDoc.GetTitle()); } catch { }
            if (sw != null)
            {
                try { sw.ExitApp(); } catch { }
                try { Marshal.FinalReleaseComObject(sw); } catch { }
            }
        }
    }
}
