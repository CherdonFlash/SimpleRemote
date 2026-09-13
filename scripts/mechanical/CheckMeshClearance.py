"""用OBJ采样点与STL射线奇偶测试检查穿壳；不替代制造级实体干涉分析。"""
import argparse
import json
from pathlib import Path
import numpy as np
from AnalyzeObjMesh import read_mesh


def stl_triangles(path):
    data = Path(path).read_bytes()
    count = int.from_bytes(data[80:84], 'little')
    if len(data) != 84 + count * 50:
        raise ValueError('需要二进制STL')
    dtype = np.dtype([('normal', '<f4', 3), ('v', '<f4', (3, 3)), ('attribute', '<u2')])
    return np.frombuffer(data, dtype=dtype, offset=84)['v'].astype(float)


def inside(points, triangles):
    # 偏移微量XY，避免射线恰好通过三角网格公共边。
    p = points + np.array([1.3e-5, 2.7e-5, 0])
    count = np.zeros(len(p), dtype=np.int32)
    boundary = np.zeros(len(p), dtype=bool)
    for t in triangles:
        a, b, c = t
        v0, v1 = b[:2] - a[:2], c[:2] - a[:2]
        den = v0[0] * v1[1] - v1[0] * v0[1]
        if abs(den) < 1e-10:
            continue
        low, high = t[:, :2].min(axis=0), t[:, :2].max(axis=0)
        ids = np.flatnonzero((p[:, 0] >= low[0]) & (p[:, 0] <= high[0]) &
                             (p[:, 1] >= low[1]) & (p[:, 1] <= high[1]))
        if not len(ids):
            continue
        delta = p[ids, :2] - a[:2]
        u = (delta[:, 0] * v1[1] - delta[:, 1] * v1[0]) / den
        v = (v0[0] * delta[:, 1] - v0[1] * delta[:, 0]) / den
        selected = (u >= 0) & (v >= 0) & (u + v <= 1)
        ids, u, v = ids[selected], u[selected], v[selected]
        z = a[2] + u * (b[2] - a[2]) + v * (c[2] - a[2])
        dz = z - p[ids, 2]
        count[ids[dz > 1e-6]] += 1
        boundary[ids[np.abs(dz) < 0.02]] = True
    return (count % 2 == 1) & ~boundary


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('archive')
    parser.add_argument('directory')
    args = parser.parse_args()
    vertices, faces = read_mesh(args.archive)
    vertices = np.array(vertices)
    ids = np.array([f[:3] for f in faces])
    # 同时取三角面重心，避免仅测顶点漏掉较大的面片。
    samples = np.vstack((vertices, vertices[ids].mean(axis=1)))
    samples += [-21.122147, -41.820067, 17.6]
    report = {'samples': len(samples), 'tolerance_mm': 0.02, 'shells': {}}
    for name, z in [('遥控器底壳.STL', 0), ('遥控器上壳.STL', 17.65)]:
        t = stl_triangles(Path(args.directory) / name)
        t[:, :, 2] += z
        low, high = t.min(axis=(0, 1)), t.max(axis=(0, 1))
        candidates = samples[np.all((samples > low) & (samples < high), axis=1)]
        failed = candidates[inside(candidates, t)]
        report['shells'][name] = dict(bounds_mm=[low.tolist(), high.tolist()],
            inside_count=len(failed),
            penetration_bounds_mm=None if not len(failed) else [failed.min(axis=0).tolist(), failed.max(axis=0).tolist()],
            example_points_mm=failed[:12].tolist())
    print(json.dumps(report, ensure_ascii=False, indent=2))
