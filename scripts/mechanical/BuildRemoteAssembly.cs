using System;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class BuildRemoteAssembly
{
    private static void Save(ModelDoc2 model, string path)
    {
        int errors = 0;
        int warnings = 0;
        bool ok = model.Extension.SaveAs(path,
            (int)swSaveAsVersion_e.swSaveAsCurrentVersion,
            (int)swSaveAsOptions_e.swSaveAsOptions_Silent,
            null, ref errors, ref warnings);
        Console.WriteLine(string.Format("save={0}|ok={1}|errors={2}|warnings={3}", path, ok, errors, warnings));
        if (!ok) throw new IOException("Failed to save assembly output; error=" + errors);
    }

    private static Component2 FindComponent(AssemblyDoc assembly, string suffix)
    {
        object[] components = (object[])assembly.GetComponents(false);
        if (components == null) return null;
        foreach (object item in components)
        {
            Component2 component = (Component2)item;
            string path = component.GetPathName();
            if (!string.IsNullOrEmpty(path) && path.EndsWith(suffix, StringComparison.OrdinalIgnoreCase))
                return component;
        }
        return null;
    }

    public static int Main(string[] args)
    {
        if (args.Length < 3)
        {
            Console.Error.WriteLine("Usage: BuildRemoteAssembly <old-assembly> <upper-part> <output-dir>");
            return 2;
        }

        string oldAssembly = Path.GetFullPath(args[0]);
        string upperPart = Path.GetFullPath(args[1]);
        string outputDir = Path.GetFullPath(args[2]);
        string outputAssembly = Path.Combine(outputDir, "遥控器完整装配体.SLDASM");
        string preview = Path.Combine(outputDir, "遥控器完整装配体_预览.png");
        string assemblyTemplate = @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_assembly.asmdot";

        SldWorks sw = null;
        ModelDoc2 model = null;
        ModelDoc2 upperModel = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            sw.SetUserPreferenceStringValue((int)swUserPreferenceStringValue_e.swDefaultTemplateAssembly, assemblyTemplate);
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swAlwaysUseDefaultTemplates, true);

            int errors = 0;
            int warnings = 0;
            upperModel = sw.OpenDoc6(upperPart,
                (int)swDocumentTypes_e.swDocPART,
                (int)swOpenDocOptions_e.swOpenDocOptions_Silent,
                "", ref errors, ref warnings);
            if (upperModel == null) throw new InvalidOperationException("Cannot preload upper-shell part.");

            errors = 0;
            warnings = 0;
            model = sw.OpenDoc6(oldAssembly,
                (int)swDocumentTypes_e.swDocASSEMBLY,
                (int)swOpenDocOptions_e.swOpenDocOptions_Silent,
                "", ref errors, ref warnings);
            Console.WriteLine(string.Format("open={0}|ok={1}|errors={2}|warnings={3}", oldAssembly, model != null, errors, warnings));
            if (model == null) return 3;

            Save(model, outputAssembly);
            AssemblyDoc assembly = (AssemblyDoc)model;
            Component2 missingBattery = FindComponent(assembly, "104060电池模型.SLDPRT");
            if (missingBattery != null)
            {
                model.ClearSelection2(true);
                bool selected = missingBattery.Select4(false, null, false);
                if (!selected)
                    selected = model.Extension.SelectByID2(missingBattery.Name2, "COMPONENT", 0, 0, 0, false, 0, null, 0);
                if (selected) model.EditDelete();
                Console.WriteLine("removeMissingBattery.selected=" + selected);
                model.ClearSelection2(true);
            }

            Component2 oldUpper = FindComponent(assembly, "遥控器上壳.SLDPRT");
            if (oldUpper != null)
            {
                oldUpper.Select4(false, null, false);
                assembly.DeleteSelections((int)swDeleteSelectionOptions_e.swDelete_Absorbed);
            }

            Component2 upper = assembly.AddComponent5(
                upperPart,
                (int)swAddComponentConfigOptions_e.swAddComponentConfigOptions_CurrentSelectedConfig,
                "", false, "", 0, 0, 0);
            if (upper == null) throw new InvalidOperationException("Cannot add upper shell to assembly.");

            // Existing lower-shell transform is identity rotation plus
            // (-96.49, +33.967, -32.675) mm.  Its mating rim is at local Z=17.600 mm;
            // add 0.05 mm assembly clearance so coincident faces do not numerically overlap.
            double[] data =
            {
                1, 0, 0,
                0, 1, 0,
                0, 0, 1,
                -0.096490, 0.033967, -0.015025,
                1, 0, 0, 0
            };
            MathUtility math = (MathUtility)sw.GetMathUtility();
            MathTransform transform = (MathTransform)math.CreateTransform(data);
            upper.Transform2 = transform;

            model.ClearSelection2(true);
            upper.Select4(false, null, false);
            assembly.FixComponent();
            model.ClearSelection2(true);
            model.ForceRebuild3(false);

            Save(model, outputAssembly);
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
            if (model != null && sw != null)
            {
                try { sw.CloseDoc(model.GetTitle()); } catch { }
            }
            if (upperModel != null && sw != null)
            {
                try { sw.CloseDoc(upperModel.GetTitle()); } catch { }
            }
            if (sw != null)
            {
                try { sw.ExitApp(); } catch { }
                try { Marshal.FinalReleaseComObject(sw); } catch { }
            }
        }
    }
}
