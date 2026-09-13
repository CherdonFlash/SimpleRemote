"""按 OBJ 的连通网格统计包络；直接读取 EDA 导出 ZIP，不修改源文件。"""
import argparse
import json
import zipfile
from collections import defaultdict


def read_mesh(path):
    """返回毫米坐标顶点与三角面，供接口截面和装配间隙检查复用。"""
    with zipfile.ZipFile(path) as archive:
        obj = next(n for n in archive.namelist() if n.lower().endswith('.obj'))
        lines = archive.read(obj).decode('utf-8-sig').splitlines()
    vertices, faces = [], []
    material = ''
    for line in lines:
        values = line.split()
        if not values:
            continue
        if values[0] == 'usemtl':
            material = values[1]
        elif values[0] == 'v':
            vertices.append(tuple(map(float, values[1:4])))
        elif values[0] == 'f':
            ids = [int(s.split('/')[0]) for s in values[1:]]
            ids = [i - 1 if i > 0 else len(vertices) + i for i in ids]
            for j in range(1, len(ids) - 1):
                faces.append((ids[0], ids[j], ids[j + 1], material))
    return vertices, faces


def analyze(path):
    with zipfile.ZipFile(path) as archive:
        obj = next(n for n in archive.namelist() if n.lower().endswith('.obj'))
        lines = archive.read(obj).decode('utf-8-sig').splitlines()
    vertices = []
    parent = []
    materials = []
    material = ''

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for line in lines:
        values = line.split()
        if not values:
            continue
        if values[0] == 'usemtl':
            material = values[1]
        elif values[0] == 'v':
            parent.append(len(vertices))
            vertices.append(tuple(map(float, values[1:4])))
            materials.append(material)
        elif values[0] == 'f':
            indices = [int(s.split('/')[0]) for s in values[1:]]
            indices = [i - 1 if i > 0 else len(vertices) + i for i in indices]
            first = root(indices[0])
            for i in indices[1:]:
                parent[root(i)] = first
    groups = defaultdict(list)
    for i in range(len(vertices)):
        groups[root(i)].append(i)
    result = []
    for ids in groups.values():
        low = [min(vertices[i][a] for i in ids) for a in range(3)]
        high = [max(vertices[i][a] for i in ids) for a in range(3)]
        result.append(dict(vertices=len(ids), material=materials[ids[0]],
                           min=low, max=high,
                           size=[high[a] - low[a] for a in range(3)],
                           center=[(high[a] + low[a]) / 2 for a in range(3)]))
    result.sort(key=lambda g: max(g['size']), reverse=True)
    return dict(source=path, obj=obj, vertex_count=len(vertices), groups=result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('archive')
    parser.add_argument('--limit', type=int, default=45)
    args = parser.parse_args()
    report = analyze(args.archive)
    report['groups'] = report['groups'][:args.limit]
    print(json.dumps(report, ensure_ascii=False, indent=2))
