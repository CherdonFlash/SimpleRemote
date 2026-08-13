using System;
using System.IO;
using System.Runtime.InteropServices;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;

internal static class CheckShellBodies
{
    private static Body2 FirstBody(ModelDoc2 model)
    {
        PartDoc part = (PartDoc)model;
        object[] bodies = (object[])part.GetBodies2((int)swBodyType_e.swSolidBody, true);
        return bodies == null || bodies.Length == 0 ? null : (Body2)bodies[0];
    }

    public static int Main(string[] args)
    {
        if (args.Length < 2) return 2;
        double assemblyOffset = args.Length >= 3
            ? double.Parse(args[2], System.Globalization.CultureInfo.InvariantCulture) * 0.001
            : 0.017600;
        SldWorks sw = null;
        ModelDoc2 lowerDoc = null, upperDoc = null;
        try
        {
            sw = new SldWorks();
            sw.Visible = false;
            sw.UserControl = false;
            int e = 0, w = 0;
            lowerDoc = sw.OpenDoc6(Path.GetFullPath(args[0]), (int)swDocumentTypes_e.swDocPART, (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref e, ref w);
            upperDoc = sw.OpenDoc6(Path.GetFullPath(args[1]), (int)swDocumentTypes_e.swDocPART, (int)swOpenDocOptions_e.swOpenDocOptions_Silent, "", ref e, ref w);
            if (lowerDoc == null || upperDoc == null) return 3;

            Body2 lower = (Body2)FirstBody(lowerDoc).Copy2(true);
            Body2 upper = (Body2)FirstBody(upperDoc).Copy2(true);
            MathUtility utility = (MathUtility)sw.GetMathUtility();
            double[] t = { 1,0,0, 0,1,0, 0,0,1, 0,0,assemblyOffset, 1,0,0,0 };
            upper.ApplyTransform((MathTransform)utility.CreateTransform(t));

            int operationError = 0;
            object raw = lower.Operations2((int)swBodyOperationType_e.SWBODYINTERSECT, upper, out operationError);
            object[] intersections = raw as object[];
            int count = intersections == null ? 0 : intersections.Length;
            double volume = 0;
            if (intersections != null)
            {
                foreach (object item in intersections)
                {
                    Body2 body = item as Body2;
                    if (body == null) continue;
                    double[] props = body.GetMassProperties(1.0) as double[];
                    if (props != null && props.Length > 3) volume += Math.Abs(props[3]);
                }
            }
            Console.WriteLine("operationError=" + operationError);
            Console.WriteLine("assemblyOffset_mm=" + (assemblyOffset * 1000.0).ToString("0.###", System.Globalization.CultureInfo.InvariantCulture));
            Console.WriteLine("intersectionBodies=" + count);
            Console.WriteLine("intersectionVolume_mm3=" + (volume * 1e9).ToString("0.######", System.Globalization.CultureInfo.InvariantCulture));
            return operationError == 0 && (count == 0 || volume < 1e-12) ? 0 : 4;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 1;
        }
        finally
        {
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
