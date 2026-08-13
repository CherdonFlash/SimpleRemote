using System;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class BuildRemoteEnclosure
{
    private const double M = 0.001;
    private const double ShellHeight = 14.5 * M;
    private const double TopThickness = 2.0 * M;

    private struct P
    {
        public double X;
        public double Y;
        public P(double x, double y) { X = x; Y = y; }
    }

    private static readonly P[] Outer =
    {
        new P(-7.635, 26.842), new P(-11.561, 23.550),
        new P(-22.663, -39.125), new P(-18.737, -43.807),
        new P(7.320, -43.807), new P(10.139, -42.640),
        new P(23.192, -29.587), new P(23.201, -29.583),
        new P(34.512, -29.583), new P(34.521, -29.587),
        new P(47.574, -42.640), new P(50.393, -43.807),
        new P(75.552, -43.807), new P(79.479, -39.125),
        new P(68.376, 23.550), new P(64.450, 26.842)
    };

    // The existing lower shell contains the exact 1 mm inward offset.  Interpolating
    // this table preserves the same sloped sides and corner centres for other offsets.
    private static readonly P[] InnerOne =
    {
        new P(-7.635, 25.842), new P(-10.576, 23.376),
        new P(-21.678, -39.299), new P(-18.737, -42.807),
        new P(7.320, -42.807), new P(9.432, -41.933),
        new P(22.485, -28.880), new P(23.201, -28.583),
        new P(34.512, -28.583), new P(35.228, -28.880),
        new P(48.281, -41.933), new P(50.393, -42.807),
        new P(75.552, -42.807), new P(78.494, -39.299),
        new P(67.391, 23.376), new P(64.450, 25.842)
    };

    private static readonly P[] ArcCenters =
    {
        new P(-7.635, 22.855), new P(-18.737, -39.820),
        new P(7.320, -39.820), new P(23.201, -29.596),
        new P(34.512, -29.596), new P(50.393, -39.820),
        new P(75.552, -39.820), new P(64.450, 22.855)
    };

    private static P At(int index, double inset)
    {
        return new P(
            Outer[index].X + inset * (InnerOne[index].X - Outer[index].X),
            Outer[index].Y + inset * (InnerOne[index].Y - Outer[index].Y));
    }

    private static void Line(SketchManager sketch, P a, P b)
    {
        sketch.CreateLine(a.X * M, a.Y * M, 0, b.X * M, b.Y * M, 0);
    }

    private static void Arc(SketchManager sketch, int centerIndex, P start, P end, short direction)
    {
        P c = ArcCenters[centerIndex];
        double v1x = start.X - c.X;
        double v1y = start.Y - c.Y;
        double v2x = end.X - c.X;
        double v2y = end.Y - c.Y;
        double r1 = Math.Sqrt(v1x * v1x + v1y * v1y);
        double r2 = Math.Sqrt(v2x * v2x + v2y * v2y);
        double ux = v1x / r1 + v2x / r2;
        double uy = v1y / r1 + v2y / r2;
        double un = Math.Sqrt(ux * ux + uy * uy);
        double radius = (r1 + r2) / 2.0;
        P middle = new P(c.X + radius * ux / un, c.Y + radius * uy / un);
        sketch.Create3PointArc(
            start.X * M, start.Y * M, 0,
            end.X * M, end.Y * M, 0,
            middle.X * M, middle.Y * M, 0);
    }

    private static void Contour(SketchManager sketch, double inset)
    {
        P[] p = new P[16];
        for (int i = 0; i < p.Length; i++) p[i] = At(i, inset);

        Arc(sketch, 0, p[0], p[1], 1);
        Line(sketch, p[1], p[2]);
        Arc(sketch, 1, p[2], p[3], 1);
        Line(sketch, p[3], p[4]);
        Arc(sketch, 2, p[4], p[5], 1);
        Line(sketch, p[5], p[6]);
        Arc(sketch, 3, p[6], p[7], -1);
        Line(sketch, p[7], p[8]);
        Arc(sketch, 4, p[8], p[9], -1);
        Line(sketch, p[9], p[10]);
        Arc(sketch, 5, p[10], p[11], 1);
        Line(sketch, p[11], p[12]);
        Arc(sketch, 6, p[12], p[13], 1);
        Line(sketch, p[13], p[14]);
        Arc(sketch, 7, p[14], p[15], 1);
        Line(sketch, p[15], p[0]);
    }

    private static Feature TopPlane(ModelDoc2 model)
    {
        Feature feature = (Feature)model.FirstFeature();
        int planeIndex = 0;
        while (feature != null)
        {
            if (feature.GetTypeName2() == "RefPlane")
            {
                // The existing lower shell was sketched on the Front plane (local XY),
                // so use the same plane to keep both native parts assembly-aligned.
                if (planeIndex == 0) return feature;
                planeIndex++;
            }
            feature = (Feature)feature.GetNextFeature();
        }
        throw new InvalidOperationException("Top reference plane was not found.");
    }

    private static void BeginSketch(ModelDoc2 model, Feature plane)
    {
        model.ClearSelection2(true);
        if (!plane.Select2(false, 0)) throw new InvalidOperationException("Cannot select top plane.");
        model.SketchManager.InsertSketch(true);
        model.SketchManager.AddToDB = true;
        model.SketchManager.DisplayWhenAdded = false;
    }

    private static Feature EndSketch(ModelDoc2 model)
    {
        model.SketchManager.AddToDB = false;
        model.SketchManager.DisplayWhenAdded = true;
        model.SketchManager.InsertSketch(true);
        model.EditRebuild3();

        Feature feature = (Feature)model.FirstFeature();
        Feature lastSketch = null;
        while (feature != null)
        {
            if (feature.GetTypeName2() == "ProfileFeature") lastSketch = feature;
            feature = (Feature)feature.GetNextFeature();
        }
        if (lastSketch == null) throw new InvalidOperationException("Created sketch was not found.");
        model.ClearSelection2(true);
        if (!lastSketch.Select2(false, 0)) throw new InvalidOperationException("Cannot select created sketch.");
        return lastSketch;
    }

    private static void BeginFaceSketchByRay(
        ModelDoc2 model,
        double rayX, double rayY, double rayZ,
        double dirX, double dirY, double dirZ)
    {
        model.ClearSelection2(true);
        bool selected = model.Extension.SelectByRay(
            rayX * M, rayY * M, rayZ * M,
            dirX, dirY, dirZ,
            0.35 * M,
            (int)swSelectType_e.swSelFACES,
            false, 0, 0);
        if (!selected) throw new InvalidOperationException("Cannot select side face for opening sketch.");
        model.SketchManager.InsertSketch(true);
        model.SketchManager.AddToDB = true;
        model.SketchManager.DisplayWhenAdded = false;
    }

    private static double[] ToSketchPoint(SldWorks sw, ModelDoc2 model, double x, double y, double z)
    {
        Sketch active = (Sketch)model.GetActiveSketch2();
        if (active == null) throw new InvalidOperationException("No active sketch.");
        MathUtility utility = (MathUtility)sw.GetMathUtility();
        MathPoint modelPoint = (MathPoint)utility.CreatePoint(new double[] { x * M, y * M, z * M });
        MathPoint sketchPoint = (MathPoint)modelPoint.MultiplyTransform(active.ModelToSketchTransform);
        return (double[])sketchPoint.ArrayData;
    }

    private static void ModelRectangle(
        SldWorks sw, ModelDoc2 model,
        double[] p0, double[] p1, double[] p2, double[] p3)
    {
        double[][] modelPoints = { p0, p1, p2, p3 };
        double[][] sketchPoints = new double[4][];
        for (int i = 0; i < 4; i++)
            sketchPoints[i] = ToSketchPoint(sw, model, modelPoints[i][0], modelPoints[i][1], modelPoints[i][2]);

        SketchManager sketch = model.SketchManager;
        for (int i = 0; i < 4; i++)
        {
            double[] a = sketchPoints[i];
            double[] b = sketchPoints[(i + 1) % 4];
            sketch.CreateLine(a[0], a[1], 0, b[0], b[1], 0);
        }
    }

    private static Feature Boss(ModelDoc2 model, double depth, bool reverse)
    {
        Feature feature = model.FeatureManager.FeatureExtrusion3(
            true, false, reverse,
            (int)swEndConditions_e.swEndCondBlind,
            (int)swEndConditions_e.swEndCondBlind,
            depth, 0,
            false, false, false, false,
            0, 0,
            false, false, false, false,
            true, true, true,
            (int)swStartConditions_e.swStartSketchPlane, 0, false);
        if (feature == null) throw new InvalidOperationException("Boss extrusion failed.");
        return feature;
    }

    private static Feature Cut(ModelDoc2 model, double depth, bool throughAll)
    {
        Feature feature = model.FeatureManager.FeatureCut4(
            true, false, true,
            throughAll ? (int)swEndConditions_e.swEndCondThroughAll : (int)swEndConditions_e.swEndCondBlind,
            (int)swEndConditions_e.swEndCondBlind,
            depth, 0,
            false, false, false, false,
            0, 0,
            false, false, false, false,
            false, true, true,
            false, false, false,
            (int)swStartConditions_e.swStartSketchPlane, 0, false,
            true);
        if (feature == null) throw new InvalidOperationException("Cut extrusion failed.");
        return feature;
    }

    private static Feature SideCut(ModelDoc2 model, double depth, string name)
    {
        Feature feature = model.FeatureManager.FeatureCut4(
            true, false, false,
            (int)swEndConditions_e.swEndCondBlind,
            (int)swEndConditions_e.swEndCondBlind,
            depth, 0,
            false, false, false, false,
            0, 0,
            false, false, false, false,
            false, true, true,
            false, false, false,
            (int)swStartConditions_e.swStartSketchPlane, 0, false,
            true);
        if (feature == null) throw new InvalidOperationException("Side opening cut failed: " + name);
        feature.Name = name;
        return feature;
    }

    private static void Circle(SketchManager sketch, double x, double y, double radius)
    {
        sketch.CreateCircleByRadius(x * M, y * M, 0, radius * M);
    }

    private static void Rectangle(SketchManager sketch, double x1, double y1, double x2, double y2)
    {
        sketch.CreateCornerRectangle(x1 * M, y1 * M, 0, x2 * M, y2 * M, 0);
    }

    private static void RotatedRectangle(SketchManager sketch, double cx, double cy, double length, double width, double angleDegrees)
    {
        double a = angleDegrees * Math.PI / 180.0;
        double ux = Math.Cos(a) * length / 2.0;
        double uy = Math.Sin(a) * length / 2.0;
        double vx = -Math.Sin(a) * width / 2.0;
        double vy = Math.Cos(a) * width / 2.0;
        P p0 = new P(cx - ux - vx, cy - uy - vy);
        P p1 = new P(cx + ux - vx, cy + uy - vy);
        P p2 = new P(cx + ux + vx, cy + uy + vy);
        P p3 = new P(cx - ux + vx, cy - uy + vy);
        Line(sketch, p0, p1);
        Line(sketch, p1, p2);
        Line(sketch, p2, p3);
        Line(sketch, p3, p0);
    }

    private static void AddProperties(ModelDoc2 model)
    {
        CustomPropertyManager props = model.Extension.get_CustomPropertyManager("");
        props.Add3("设计用途", (int)swCustomInfoType_e.swCustomInfoText, "SimpleRemote 遥控器上壳", (int)swCustomPropertyAddOption_e.swCustomPropertyReplaceValue);
        props.Add3("基准PCB", (int)swCustomInfoType_e.swCustomInfoText, "3D_PCB1_2026-08-11.obj / latest 21:41 export / corrected joysticks", (int)swCustomPropertyAddOption_e.swCustomPropertyReplaceValue);
        props.Add3("打印建议", (int)swCustomInfoType_e.swCustomInfoText, "PETG/ABS, 0.20 mm layer, 4 walls", (int)swCustomPropertyAddOption_e.swCustomPropertyReplaceValue);
        props.Add3("设计间隙", (int)swCustomInfoType_e.swCustomInfoText, "定位舌至PCB约 0.25 mm；接口孔按2026-08-11 OBJ并预留0.4 mm", (int)swCustomPropertyAddOption_e.swCustomPropertyReplaceValue);
    }

    private static void Save(ModelDoc2 model, string path)
    {
        int errors = 0;
        int warnings = 0;
        bool ok = model.Extension.SaveAs(path,
            (int)swSaveAsVersion_e.swSaveAsCurrentVersion,
            (int)swSaveAsOptions_e.swSaveAsOptions_Silent,
            null, ref errors, ref warnings);
        Console.WriteLine(string.Format("save={0}|ok={1}|errors={2}|warnings={3}", path, ok, errors, warnings));
        if (!ok) throw new IOException("Failed to save " + path + "; error=" + errors);
    }

    private static void SavePreview(ModelDoc2 model, string path)
    {
        model.ShowNamedView2("*Isometric", (int)swStandardViews_e.swIsometricView);
        model.ViewZoomtofit2();
        Save(model, path);
    }

    private static void SaveView(ModelDoc2 model, string viewName, int viewId, string path)
    {
        model.ShowNamedView2(viewName, viewId);
        model.ViewZoomtofit2();
        Save(model, path);
    }

    public static int Main(string[] args)
    {
        string outputDir = args.Length > 0 ? Path.GetFullPath(args[0]) : System.Environment.CurrentDirectory;
        Directory.CreateDirectory(outputDir);

        string partPath = Path.Combine(outputDir, "遥控器上壳.SLDPRT");
        string stepPath = Path.Combine(outputDir, "遥控器上壳.STEP");
        string stlPath = Path.Combine(outputDir, "遥控器上壳.STL");
        string previewPath = Path.Combine(outputDir, "遥控器上壳_预览.png");
        string frontViewPath = Path.Combine(outputDir, "遥控器上壳_俯视图.png");
        string typeCViewPath = Path.Combine(outputDir, "遥控器上壳_TypeC侧视图.png");
        string switchViewPath = Path.Combine(outputDir, "遥控器上壳_电源开关侧视图.png");
        string nrfViewPath = Path.Combine(outputDir, "遥控器上壳_NRF24侧视图.png");
        string template = @"C:\ProgramData\SOLIDWORKS\SOLIDWORKS 2024\templates\gb_part.prtdot";

        SldWorks sw = null;
        ModelDoc2 model = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            sw.SetUserPreferenceStringValue((int)swUserPreferenceStringValue_e.swDefaultTemplatePart, template);
            sw.SetUserPreferenceToggle((int)swUserPreferenceToggle_e.swAlwaysUseDefaultTemplates, true);

            model = (ModelDoc2)sw.NewDocument(template, 0, 0, 0);
            if (model == null) throw new InvalidOperationException("Cannot create SolidWorks part document.");
            Feature topPlane = TopPlane(model);

            // 1. Full outside volume.
            BeginSketch(model, topPlane);
            Contour(model.SketchManager, 0.0);
            EndSketch(model);
            Feature outer = Boss(model, ShellHeight, false);
            outer.Name = "上壳外形_高14.5";

            // 2. Hollow the part from the mating face, leaving 2 mm top and 2 mm side walls.
            BeginSketch(model, topPlane);
            Contour(model.SketchManager, 2.0);
            EndSketch(model);
            Feature cavity = Cut(model, ShellHeight - TopThickness, false);
            cavity.Name = "内部腔体_壁厚2.0";

            // 3. Four internal screw bosses aligned with the existing lower-shell posts.
            BeginSketch(model, topPlane);
            Circle(model.SketchManager, -3.469, 1.614, 3.50);
            Circle(model.SketchManager, 66.000, 9.107, 3.50);
            Circle(model.SketchManager, -2.822, -37.375, 3.50);
            Circle(model.SketchManager, 59.778, -37.375, 3.50);
            EndSketch(model);
            Feature bosses = Boss(model, ShellHeight, false);
            bosses.Name = "螺钉柱_对齐旧底壳";

            BeginSketch(model, topPlane);
            Circle(model.SketchManager, -3.469, 1.614, 1.25);
            Circle(model.SketchManager, 66.000, 9.107, 1.25);
            Circle(model.SketchManager, -2.822, -37.375, 1.25);
            Circle(model.SketchManager, 59.778, -37.375, 1.25);
            EndSketch(model);
            Feature pilots = Cut(model, ShellHeight + 2 * M, true);
            pilots.Name = "M3自攻底孔_直径2.5";

            // 4. Front-panel controls. USB-C, SW13 and NRF24 are intentionally excluded here:
            // they are opened only through their connector-facing side walls below.
            BeginSketch(model, topPlane);
            // Corrected 2026-08-11 OBJ joystick shaft centres at the upper-shell face
            // (OBJ Z=16.15 mm), mapped with Xshell=Xobj-21.122147 and
            // Yshell=Yobj-41.820067. Keep the existing 21.5 mm opening diameter.
            Circle(model.SketchManager, -2.427, -13.356, 10.75);
            Circle(model.SketchManager, 59.041, -13.356, 10.75);

            Circle(model.SketchManager, -11.089, -35.723, 4.10);
            Circle(model.SketchManager, 7.669, -35.723, 4.10);
            Circle(model.SketchManager, 51.053, -35.723, 4.10);
            Circle(model.SketchManager, 69.810, -35.723, 4.10);

            Circle(model.SketchManager, 59.147, 12.097, 3.60);

            EndSketch(model);
            Feature openings = Cut(model, ShellHeight + 5 * M, true);
            openings.Name = "FrontPanel_Control_Openings";

            // 0.96-inch OLED: user-confirmed module size is 25.2 mm wide and extends
            // 26.0 mm downward from the female-header edge toward USB Type-C. The header's
            // NRF24-side edge is Y=-1.834 mm; centre the module on the 29.805 mm header X.
            BeginSketch(model, topPlane);
            Rectangle(model.SketchManager, 17.205, -27.834, 42.405, -1.834);
            EndSketch(model);
            Feature oledOpening = Cut(model, ShellHeight + 5 * M, true);
            oledOpening.Name = "OLED_0p96_From_Header_Toward_TypeC";

            // 5. Latest-PCB side openings. PCB-to-shell transform is centre aligned:
            // Xshell = Xpcb - 21.122147, Yshell = Ypcb - 41.820067.
            // Board top is aligned to the 17.6 mm lower-shell rim plane.

            // USB1 / Type-C: only the recessed connector-facing wall is opened. The cut is
            // 9.54 x 3.75 mm including print/assembly clearance and does not touch the front face.
            BeginFaceSketchByRay(model, 28.85, -35.0, 1.8, 0, 1, 0);
            ModelRectangle(sw, model,
                new double[] { 23.86, -29.583, -0.30 },
                new double[] { 33.41, -29.583, -0.30 },
                new double[] { 33.41, -29.583, 3.45 },
                new double[] { 23.86, -29.583, 3.45 });
            EndSketch(model);
            SideCut(model, 4.0 * M, "USB_TypeC_Side_Opening");

            // SW13 / power switch: only the black toggle crosses the sloped left wall.
            // Its corrected-OBJ wall intersection is 3.90 x 3.90 mm. The following
            // The visible toggle is 3.90 mm wide and moves about 2.14 mm between ON/OFF.
            // A 6.65 x 4.50 mm slot covers both end positions plus about 0.30 mm clearance,
            // while the top-view/front face remains completely closed above the switch.
            BeginFaceSketchByRay(model, -21.16, 15.24, 2.0, 0.984671, -0.174421, 0);
            ModelRectangle(sw, model,
                new double[] { -12.700, 17.120, -0.55 },
                new double[] { -13.860, 10.572, -0.55 },
                new double[] { -13.860, 10.572, 3.95 },
                new double[] { -12.700, 17.120, 3.95 });
            EndSketch(model);
            SideCut(model, 4.0 * M, "SW13_Toggle_Left_Side_Opening_2026-08-11_OBJ");

            // U2 / NRF24 from the corrected 2026-08-11 OBJ.  The module is rotated 90 degrees
            // in the PCB plane: its rear-wall projection is 18.0 x 6.66 mm, not 43.05 x 12.8 mm.
            BeginFaceSketchByRay(model, 28.81, 34.0, 9.8, 0, -1, 0);
            ModelRectangle(sw, model,
                new double[] { 19.41, 26.842, 6.09 },
                new double[] { 38.21, 26.842, 6.09 },
                new double[] { 38.21, 26.842, 13.55 },
                new double[] { 19.41, 26.842, 13.55 });
            EndSketch(model);
            SideCut(model, 4.0 * M, "NRF24_Corrected_2026-08-11_Rear_Opening");

            // 6. 1.2 mm locating tongue.  The latest PCB sits 2.0 mm inside the shell;
            // keep the tongue between 1.25 and 1.75 mm inset for 0.25 mm board clearance.
            BeginSketch(model, topPlane);
            Contour(model.SketchManager, 1.25);
            Contour(model.SketchManager, 1.75);
            EndSketch(model);
            Feature tongue = Boss(model, 1.2 * M, true);
            tongue.Name = "内定位舌_避让最新PCB_0.25";

            AddProperties(model);
            model.ForceRebuild3(false);

            Save(model, partPath);
            Save(model, stepPath);
            Save(model, stlPath);
            SavePreview(model, previewPath);
            SaveView(model, "*Front", (int)swStandardViews_e.swFrontView, frontViewPath);
            SaveView(model, "*Bottom", (int)swStandardViews_e.swBottomView, typeCViewPath);
            SaveView(model, "*Left", (int)swStandardViews_e.swLeftView, switchViewPath);
            SaveView(model, "*Top", (int)swStandardViews_e.swTopView, nrfViewPath);

            PartDoc part = (PartDoc)model;
            double[] box = (double[])part.GetPartBox(true);
            Console.WriteLine(string.Format(CultureInfo.InvariantCulture,
                "box_mm={0:0.###},{1:0.###},{2:0.###}",
                (box[3] - box[0]) * 1000.0,
                (box[4] - box[1]) * 1000.0,
                (box[5] - box[2]) * 1000.0));
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
