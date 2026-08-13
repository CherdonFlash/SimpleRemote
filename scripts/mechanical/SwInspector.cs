using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class SwInspector
{
    private static string Mm(double meters)
    {
        return (meters * 1000.0).ToString("0.######", CultureInfo.InvariantCulture);
    }

    private static void PrintBox(string label, double[] box)
    {
        if (box == null || box.Length < 6)
        {
            Console.WriteLine(label + "=<none>");
            return;
        }

        Console.WriteLine(string.Format("{0}.min_mm={1},{2},{3}", label, Mm(box[0]), Mm(box[1]), Mm(box[2])));
        Console.WriteLine(string.Format("{0}.max_mm={1},{2},{3}", label, Mm(box[3]), Mm(box[4]), Mm(box[5])));
        Console.WriteLine(string.Format("{0}.size_mm={1},{2},{3}", label, Mm(box[3] - box[0]), Mm(box[4] - box[1]), Mm(box[5] - box[2])));
    }

    private static void PrintFeatures(ModelDoc2 model)
    {
        Console.WriteLine("features.begin");
        Feature feature = (Feature)model.FirstFeature();
        int count = 0;
        while (feature != null && count < 500)
        {
            Console.WriteLine(string.Format("feature={0}|{1}|{2}", count, feature.Name, feature.GetTypeName2()));
            if (feature.GetTypeName2() == "ProfileFeature")
                PrintSketch(feature);
            else if (feature.GetTypeName2() == "Extrusion")
                PrintExtrusion(feature);
            feature = (Feature)feature.GetNextFeature();
            count++;
        }
        Console.WriteLine("features.end");
    }

    private static string PointText(SketchPoint point)
    {
        return string.Format(CultureInfo.InvariantCulture, "{0:0.###},{1:0.###},{2:0.###}", point.X * 1000.0, point.Y * 1000.0, point.Z * 1000.0);
    }

    private static void PrintSketch(Feature feature)
    {
        try
        {
            Sketch sketch = (Sketch)feature.GetSpecificFeature2();
            object[] segments = (object[])sketch.GetSketchSegments();
            Console.WriteLine(string.Format("sketch={0}|segments={1}", feature.Name, segments == null ? 0 : segments.Length));
            if (segments == null) return;

            for (int i = 0; i < segments.Length; i++)
            {
                SketchSegment segment = (SketchSegment)segments[i];
                int type = segment.GetType();
                if (type == (int)swSketchSegments_e.swSketchLINE)
                {
                    SketchLine line = (SketchLine)segment;
                    Console.WriteLine(string.Format("segment={0}|line|{1}|{2}|construction={3}", i,
                        PointText((SketchPoint)line.GetStartPoint2()),
                        PointText((SketchPoint)line.GetEndPoint2()),
                        segment.ConstructionGeometry));
                }
                else if (type == (int)swSketchSegments_e.swSketchARC)
                {
                    SketchArc arc = (SketchArc)segment;
                    Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                        "segment={0}|arc|start={1}|end={2}|center={3}|radius_mm={4:0.###}|circle={5}|dir={6}|construction={7}",
                        i,
                        PointText((SketchPoint)arc.GetStartPoint2()),
                        PointText((SketchPoint)arc.GetEndPoint2()),
                        PointText((SketchPoint)arc.GetCenterPoint2()),
                        arc.GetRadius() * 1000.0,
                        arc.IsCircle(),
                        arc.GetRotationDir(),
                        segment.ConstructionGeometry));
                }
                else
                {
                    Console.WriteLine(string.Format("segment={0}|type={1}|construction={2}", i, type, segment.ConstructionGeometry));
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine("sketchError=" + ex.Message);
        }
    }

    private static void PrintExtrusion(Feature feature)
    {
        try
        {
            ExtrudeFeatureData2 definition = (ExtrudeFeatureData2)feature.GetDefinition();
            Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                "extrusion={0}|depth1_mm={1:0.###}|depth2_mm={2:0.###}|reverse={3}|both={4}|thin={5}|draft_deg={6:0.###}",
                feature.Name,
                definition.GetDepth(true) * 1000.0,
                definition.GetDepth(false) * 1000.0,
                definition.ReverseDirection,
                definition.BothDirections,
                definition.IsThinFeature(),
                definition.GetDraftAngle(true) * 180.0 / Math.PI));
        }
        catch (Exception ex)
        {
            Console.WriteLine("extrusionError=" + ex.Message);
        }
    }

    private static void PrintPart(ModelDoc2 model)
    {
        PartDoc part = (PartDoc)model;
        PrintBox("partBox", (double[])part.GetPartBox(true));

        object[] bodies = (object[])part.GetBodies2((int)swBodyType_e.swAllBodies, true);
        Console.WriteLine("bodyCount=" + (bodies == null ? 0 : bodies.Length));
        if (bodies != null)
        {
            for (int i = 0; i < bodies.Length; i++)
            {
                Body2 body = (Body2)bodies[i];
                PrintBox("body" + i + "Box", (double[])body.GetBodyBox());
                object[] faces = (object[])body.GetFaces();
                Console.WriteLine(string.Format("body={0}|{1}|faces={2}", i, body.Name, faces == null ? 0 : faces.Length));
            }
        }
    }

    private static void PrintAssembly(ModelDoc2 model)
    {
        AssemblyDoc assembly = (AssemblyDoc)model;
        object[] components = (object[])assembly.GetComponents(false);
        Console.WriteLine("componentCount=" + (components == null ? 0 : components.Length));
        if (components == null) return;

        for (int i = 0; i < components.Length; i++)
        {
            Component2 component = (Component2)components[i];
            Console.WriteLine(string.Format("component={0}|{1}|{2}|suppressed={3}", i, component.Name2, component.GetPathName(), component.IsSuppressed()));
            try
            {
                PrintBox("component" + i + "Box", (double[])component.GetBox(false, false));
            }
            catch (Exception ex)
            {
                Console.WriteLine("componentBoxError=" + ex.Message);
            }
        }
    }

    private static void SaveView(ModelDoc2 model, string outputPng, int viewId)
    {
        model.ShowNamedView2("*Isometric", viewId);
        model.ViewZoomtofit2();
        int errors = 0;
        int warnings = 0;
        bool ok = model.Extension.SaveAs(
            outputPng,
            (int)swSaveAsVersion_e.swSaveAsCurrentVersion,
            (int)swSaveAsOptions_e.swSaveAsOptions_Silent,
            null,
            ref errors,
            ref warnings);
        Console.WriteLine(string.Format("image={0}|ok={1}|errors={2}|warnings={3}", outputPng, ok, errors, warnings));
    }

    public static int Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.Error.WriteLine("Usage: SwInspector <cad-file> [output.png]");
            return 2;
        }

        string path = Path.GetFullPath(args[0]);
        string extension = Path.GetExtension(path).ToLowerInvariant();
        int docType = extension == ".sldasm" ? (int)swDocumentTypes_e.swDocASSEMBLY : (int)swDocumentTypes_e.swDocPART;

        SldWorks sw = null;
        ModelDoc2 model = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;

            sw.SetUserPreferenceStringValue(
                (int)swUserPreferenceStringValue_e.swDefaultTemplatePart,
                @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_part.prtdot");
            sw.SetUserPreferenceStringValue(
                (int)swUserPreferenceStringValue_e.swDefaultTemplateAssembly,
                @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_assembly.asmdot");
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swAlwaysUseDefaultTemplates, true);
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swMultiCAD_Enable3DInterconnect, true);

            int errors = 0;
            int warnings = 0;
            if (extension == ".step" || extension == ".stp" || extension == ".iges" || extension == ".igs" || extension == ".obj")
            {
                model = sw.LoadFile4(path, "", null, ref errors);
            }
            else
            {
                model = sw.OpenDoc6(path, docType, (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref errors, ref warnings);
            }

            Console.WriteLine(string.Format("open={0}|errors={1}|warnings={2}|ok={3}", path, errors, warnings, model != null));
            if (model == null) return 3;

            Console.WriteLine("title=" + model.GetTitle());
            Console.WriteLine("path=" + model.GetPathName());
            Console.WriteLine("type=" + model.GetType());

            if (model.GetType() == (int)swDocumentTypes_e.swDocPART)
                PrintPart(model);
            else if (model.GetType() == (int)swDocumentTypes_e.swDocASSEMBLY)
                PrintAssembly(model);

            PrintFeatures(model);
            if (args.Length >= 2)
                SaveView(model, Path.GetFullPath(args[1]), (int)swStandardViews_e.swIsometricView);

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
            if (sw != null)
            {
                try { sw.ExitApp(); } catch { }
                try { Marshal.FinalReleaseComObject(sw); } catch { }
            }
        }
    }
}
