using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class BuildNewPcbValidation
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

    private static Feature FirstPlane(ModelDoc2 model)
    {
        Feature feature = (Feature)model.FirstFeature();
        while (feature != null)
        {
            if (feature.GetTypeName2() == "RefPlane") return feature;
            feature = (Feature)feature.GetNextFeature();
        }
        throw new InvalidOperationException("Front plane not found.");
    }

    private static string BuildBox(SldWorks sw, string outputDir, string name, double x, double y, double z)
    {
        string path = Path.Combine(outputDir, name + ".SLDPRT");
        ModelDoc2 model = (ModelDoc2)sw.NewDocument(PartTemplate, 0, 0, 0);
        if (model == null) throw new InvalidOperationException("Cannot create envelope part: " + name);
        try
        {
            Feature plane = FirstPlane(model);
            model.ClearSelection2(true);
            plane.Select2(false, 0);
            model.SketchManager.InsertSketch(true);
            model.SketchManager.CreateCornerRectangle(0, 0, 0, x * M, y * M, 0);
            model.SketchManager.InsertSketch(true);
            Feature boss = model.FeatureManager.FeatureExtrusion3(
                true, false, false,
                (int)swEndConditions_e.swEndCondBlind,
                (int)swEndConditions_e.swEndCondBlind,
                z * M, 0,
                false, false, false, false,
                0, 0,
                false, false, false, false,
                true, true, true,
                (int)swStartConditions_e.swStartSketchPlane, 0, false);
            if (boss == null) throw new InvalidOperationException("Cannot extrude envelope: " + name);
            boss.Name = name + "_Latest_STEP_Envelope";
            Save(model, path);
        }
        finally
        {
            sw.CloseDoc(model.GetTitle());
        }
        return path;
    }

    private static string ImportStepPart(SldWorks sw, string stepPath, string outputPath)
    {
        int errors = 0;
        ModelDoc2 imported = sw.LoadFile4(stepPath, "", null, ref errors);
        if (imported == null) throw new InvalidOperationException("Cannot import STEP subset: " + stepPath + "; error=" + errors);
        try
        {
            Save(imported, outputPath);
        }
        finally
        {
            sw.CloseDoc(imported.GetTitle());
        }
        return outputPath;
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

    private static MathTransform Placement(
        SldWorks sw, double[] xAxis, double[] yAxis, double[] zAxis,
        double xMm, double yMm, double zMm)
    {
        double[] values =
        {
            xAxis[0], xAxis[1], xAxis[2],
            yAxis[0], yAxis[1], yAxis[2],
            zAxis[0], zAxis[1], zAxis[2],
            xMm * M, yMm * M, zMm * M,
            1,0,0,0
        };
        return (MathTransform)((MathUtility)sw.GetMathUtility()).CreateTransform(values);
    }

    private static Component2 AddFixed(
        SldWorks sw, ModelDoc2 model, AssemblyDoc assembly,
        string path, double x, double y, double z, double[] colour)
    {
        Component2 component = assembly.AddComponent5(path,
            (int)swAddComponentConfigOptions_e.swAddComponentConfigOptions_CurrentSelectedConfig,
            "", false, "", 0, 0, 0);
        if (component == null) throw new InvalidOperationException("Cannot add component: " + path);
        component.Transform2 = Translation(sw, x, y, z);
        if (colour != null) component.MaterialPropertyValues = colour;
        model.ClearSelection2(true);
        component.Select4(false, null, false);
        assembly.FixComponent();
        model.ClearSelection2(true);
        return component;
    }

    private static Component2 AddFixedPlacement(
        SldWorks sw, ModelDoc2 model, AssemblyDoc assembly, string path,
        double[] xAxis, double[] yAxis, double[] zAxis,
        double x, double y, double z, double[] colour)
    {
        Component2 component = assembly.AddComponent5(path,
            (int)swAddComponentConfigOptions_e.swAddComponentConfigOptions_CurrentSelectedConfig,
            "", false, "", 0, 0, 0);
        if (component == null) throw new InvalidOperationException("Cannot add component: " + path);
        component.Transform2 = Placement(sw, xAxis, yAxis, zAxis, x, y, z);
        if (colour != null) component.MaterialPropertyValues = colour;
        model.ClearSelection2(true);
        component.Select4(false, null, false);
        assembly.FixComponent();
        model.ClearSelection2(true);
        return component;
    }

    public static int Main(string[] args)
    {
        if (args.Length < 4)
        {
            Console.Error.WriteLine("Usage: BuildNewPcbValidation <board-step> <lower-part> <upper-part> <output-dir>");
            return 2;
        }

        string boardStep = Path.GetFullPath(args[0]);
        string lowerPath = Path.GetFullPath(args[1]);
        string upperPath = Path.GetFullPath(args[2]);
        string outputDir = Path.GetFullPath(args[3]);
        string boardPart = Path.Combine(outputDir, "PCB1_2026-08-10_板体.SLDPRT");
        string criticalDir = Path.Combine(Path.GetDirectoryName(boardStep), "critical_step");
        string typeCStep = Path.Combine(criticalDir, "PCB1_2026-08-10_USB_TypeC.step");
        string switchStep = Path.Combine(criticalDir, "PCB1_2026-08-10_SW13.step");
        string nrfStep = Path.Combine(criticalDir, "PCB1_2026-08-10_NRF24_U2.step");
        string typeCPart = Path.Combine(outputDir, "PCB1_2026-08-10_USB_TypeC.SLDPRT");
        string switchPart = Path.Combine(outputDir, "PCB1_2026-08-10_SW13.SLDPRT");
        string nrfPart = Path.Combine(outputDir, "PCB1_2026-08-10_NRF24_U2.SLDPRT");
        string assemblyPath = Path.Combine(outputDir, "遥控器新PCB校核装配体.SLDASM");
        string previewPath = Path.Combine(outputDir, "遥控器新PCB校核装配体_预览.png");
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

            ImportStepPart(sw, boardStep, boardPart);
            ImportStepPart(sw, typeCStep, typeCPart);
            ImportStepPart(sw, switchStep, switchPart);
            ImportStepPart(sw, nrfStep, nrfPart);

            foreach (string part in new[] { lowerPath, upperPath, boardPart, typeCPart, switchPart, nrfPart })
            {
                int openErrors = 0, warnings = 0;
                ModelDoc2 loaded = sw.OpenDoc6(part, (int)swDocumentTypes_e.swDocPART,
                    (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref openErrors, ref warnings);
                if (loaded == null) throw new InvalidOperationException("Cannot preload part: " + part);
                preloaded.Add(loaded);
            }

            assemblyModel = (ModelDoc2)sw.NewDocument(AssemblyTemplate, 0, 0, 0);
            if (assemblyModel == null) throw new InvalidOperationException("Cannot create validation assembly.");
            AssemblyDoc assembly = (AssemblyDoc)assemblyModel;

            double[] shellColour = { 0.78, 0.78, 0.82, 0.45, 0.55, 0.35, 0.25, 0.20, 0.0 };
            double[] boardColour = { 0.05, 0.35, 0.12, 0.35, 0.45, 0.35, 0.20, 0.15, 0.0 };
            double[] usbColour = { 0.65, 0.68, 0.72, 0.35, 0.55, 0.55, 0.25, 0.25, 0.0 };
            double[] switchColour = { 0.12, 0.12, 0.12, 0.35, 0.35, 0.35, 0.15, 0.25, 0.0 };
            double[] nrfColour = { 0.10, 0.28, 0.58, 0.35, 0.50, 0.35, 0.20, 0.20, 0.0 };

            AddFixed(sw, assemblyModel, assembly, lowerPath, 0, 0, 0, shellColour);
            AddFixed(sw, assemblyModel, assembly, upperPath, 0, 0, 17.65, shellColour);

            // Exact centre alignment from the extracted latest Board body:
            // board box = (0.398124..98.662075, 0.012700..66.662433) mm.
            const double tx = -21.122147;
            const double ty = -41.820067;
            const double tz = 16.0;
            AddFixed(sw, assemblyModel, assembly, boardPart, tx, ty, tz, boardColour);

            // Exact local bodies and placement axes extracted from PCB1_2026-08-10.step.
            double[] axisX = { 1, 0, 0 };
            double[] axisY = { 0, 1, 0 };
            double[] axisZ = { 0, 0, 1 };
            AddFixedPlacement(sw, assemblyModel, assembly, typeCPart,
                axisX, axisY, axisZ,
                tx + 47.030062, ty + 12.887083, tz + 5.504059, usbColour);

            double[] switchX = { 0.161955178, 0.986798115, 0 };
            double[] switchY = { -0.986798115, 0.161955178, 0 };
            AddFixedPlacement(sw, assemblyModel, assembly, switchPart,
                switchX, switchY, axisZ,
                tx + 10.007176, ty + 55.281607, tz + 4.950046, switchColour);

            double[] nrfX = { 0, -1, 0 };
            double[] nrfY = { 1, 0, 0 };
            AddFixedPlacement(sw, assemblyModel, assembly, nrfPart,
                nrfX, nrfY, axisZ,
                tx + 40.929797, ty + 94.210149, tz + 11.140053, nrfColour);

            assemblyModel.ForceRebuild3(false);
            Save(assemblyModel, assemblyPath);
            assemblyModel.ShowNamedView2("*Isometric", (int)swStandardViews_e.swIsometricView);
            assemblyModel.ViewZoomtofit2();
            Save(assemblyModel, previewPath);
            assemblyModel.ShowNamedView2("*Bottom", (int)swStandardViews_e.swBottomView);
            assemblyModel.ViewZoomtofit2();
            Save(assemblyModel, typeCViewPath);
            assemblyModel.ShowNamedView2("*Left", (int)swStandardViews_e.swLeftView);
            assemblyModel.ViewZoomtofit2();
            Save(assemblyModel, switchViewPath);
            assemblyModel.ShowNamedView2("*Top", (int)swStandardViews_e.swTopView);
            assemblyModel.ViewZoomtofit2();
            Save(assemblyModel, nrfViewPath);
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
