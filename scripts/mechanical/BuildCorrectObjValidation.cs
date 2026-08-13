using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class BuildCorrectObjValidation
{
    private const double M = 0.001;
    private const string PartTemplate = @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_part.prtdot";
    private const string AssemblyTemplate = @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_assembly.asmdot";

    private static void Save(ModelDoc2 model, string path)
    {
        int errors = 0, warnings = 0;
        bool ok = model.Extension.SaveAs(path,
            (int)swSaveAsVersion_e.swSaveAsCurrentVersion,
            (int)swSaveAsOptions_e.swSaveAsOptions_Silent,
            null, ref errors, ref warnings);
        Console.WriteLine(string.Format("save={0}|ok={1}|errors={2}|warnings={3}", path, ok, errors, warnings));
        if (!ok) throw new IOException("Save failed: " + path + "; error=" + errors);
    }

    private static MathTransform Translation(SldWorks sw, double xMm, double yMm, double zMm)
    {
        double[] values =
        {
            1,0,0, 0,1,0, 0,0,1,
            xMm * M, yMm * M, zMm * M,
            1,0,0,0
        };
        return (MathTransform)((MathUtility)sw.GetMathUtility()).CreateTransform(values);
    }

    private static Component2 AddFixed(
        SldWorks sw, ModelDoc2 model, AssemblyDoc assembly,
        string path, double x, double y, double z)
    {
        Component2 component = assembly.AddComponent5(path,
            (int)swAddComponentConfigOptions_e.swAddComponentConfigOptions_CurrentSelectedConfig,
            "", false, "", 0, 0, 0);
        if (component == null) throw new InvalidOperationException("Cannot add component: " + path);
        component.Transform2 = Translation(sw, x, y, z);
        model.ClearSelection2(true);
        component.Select4(false, null, false);
        assembly.FixComponent();
        model.ClearSelection2(true);
        return component;
    }

    private static void SaveView(ModelDoc2 model, string viewName, int viewId, string path)
    {
        model.ShowNamedView2(viewName, viewId);
        model.ViewZoomtofit2();
        Save(model, path);
    }

    public static int Main(string[] args)
    {
        if (args.Length < 4)
        {
            Console.Error.WriteLine("Usage: BuildCorrectObjValidation <obj> <lower-part> <upper-part> <output-dir>");
            return 2;
        }

        string objPath = Path.GetFullPath(args[0]);
        string lowerPath = Path.GetFullPath(args[1]);
        string upperPath = Path.GetFullPath(args[2]);
        string outputDir = Path.GetFullPath(args[3]);
        string pcbPart = Path.Combine(outputDir, "3D_PCB1_2026-08-11.SLDPRT");
        string pcbPreviewPath = Path.Combine(outputDir, "3D_PCB1_2026-08-11_预览.png");
        string pcbPreviewZoomPath = Path.Combine(outputDir, "3D_PCB1_2026-08-11_preview_zoom.png");
        string assemblyPath = Path.Combine(outputDir, "遥控器新PCB校核装配体.SLDASM");
        string previewPath = Path.Combine(outputDir, "遥控器新PCB校核装配体_预览.png");
        string frontViewPath = Path.Combine(outputDir, "遥控器新PCB校核装配体_俯视图.png");
        string typeCViewPath = Path.Combine(outputDir, "遥控器新PCB校核装配体_TypeC侧视图.png");
        string switchViewPath = Path.Combine(outputDir, "遥控器新PCB校核装配体_电源开关侧视图.png");
        string nrfViewPath = Path.Combine(outputDir, "遥控器新PCB校核装配体_NRF24侧视图.png");

        SldWorks sw = null;
        ModelDoc2 assemblyModel = null;
        var preloaded = new List<ModelDoc2>();
        try
        {
            Directory.CreateDirectory(outputDir);
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            sw.SetUserPreferenceStringValue((int)swUserPreferenceStringValue_e.swDefaultTemplatePart, PartTemplate);
            sw.SetUserPreferenceStringValue((int)swUserPreferenceStringValue_e.swDefaultTemplateAssembly, AssemblyTemplate);
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swAlwaysUseDefaultTemplates, true);
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swMultiCAD_Enable3DInterconnect, true);

            int importErrors = 0;
            ModelDoc2 imported = sw.LoadFile4(objPath, "", null, ref importErrors);
            if (imported == null) throw new InvalidOperationException("Cannot import corrected OBJ; error=" + importErrors);
            Save(imported, pcbPart);
            SaveView(imported, "*Isometric", (int)swStandardViews_e.swIsometricView, pcbPreviewPath);
            SaveView(imported, "*Isometric", (int)swStandardViews_e.swIsometricView, pcbPreviewZoomPath);
            sw.CloseDoc(imported.GetTitle());

            foreach (string part in new[] { lowerPath, upperPath, pcbPart })
            {
                int errors = 0, warnings = 0;
                ModelDoc2 loaded = sw.OpenDoc6(part, (int)swDocumentTypes_e.swDocPART,
                    (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref errors, ref warnings);
                if (loaded == null) throw new InvalidOperationException("Cannot preload part: " + part);
                preloaded.Add(loaded);
            }

            assemblyModel = (ModelDoc2)sw.NewDocument(AssemblyTemplate, 0, 0, 0);
            if (assemblyModel == null) throw new InvalidOperationException("Cannot create corrected PCB validation assembly.");
            AssemblyDoc assembly = (AssemblyDoc)assemblyModel;
            AddFixed(sw, assemblyModel, assembly, lowerPath, 0, 0, 0);
            AddFixed(sw, assemblyModel, assembly, upperPath, 0, 0, 17.65);

            // Main-board bounds in the corrected OBJ are still 98.263 x 66.650 mm,
            // so retain the proven centre alignment to the original lower shell.
            AddFixed(sw, assemblyModel, assembly, pcbPart, -21.122147, -41.820067, 16.0);

            assemblyModel.ForceRebuild3(false);
            Save(assemblyModel, assemblyPath);
            SaveView(assemblyModel, "*Isometric", (int)swStandardViews_e.swIsometricView, previewPath);
            SaveView(assemblyModel, "*Front", (int)swStandardViews_e.swFrontView, frontViewPath);
            SaveView(assemblyModel, "*Bottom", (int)swStandardViews_e.swBottomView, typeCViewPath);
            SaveView(assemblyModel, "*Left", (int)swStandardViews_e.swLeftView, switchViewPath);
            SaveView(assemblyModel, "*Top", (int)swStandardViews_e.swTopView, nrfViewPath);
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 1;
        }
        finally
        {
            if (assemblyModel != null && sw != null) try { sw.CloseDoc(assemblyModel.GetTitle()); } catch { }
            if (sw != null)
            {
                foreach (ModelDoc2 document in preloaded)
                    try { sw.CloseDoc(document.GetTitle()); } catch { }
                try { sw.ExitApp(); } catch { }
                try { Marshal.FinalReleaseComObject(sw); } catch { }
            }
        }
    }
}
