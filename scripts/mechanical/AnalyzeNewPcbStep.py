#!/usr/bin/env python3
"""Extract mechanical envelopes from the latest EasyEDA STEP export.

This intentionally reads the STEP entity graph directly.  It keeps SolidWorks
out of the critical measurement path, because importing the complete 45 MB
assembly through 3D Interconnect is unnecessarily slow and unreliable.
"""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path


ENTITY_RE = re.compile(r"^\s*#(\d+)\s*=\s*(.*)\s*$", re.S)
REF_RE = re.compile(r"#(\d+)")
XYZ_RE = re.compile(
    r"CARTESIAN_POINT\s*\([^,]*,\s*\(\s*"
    r"([-+0-9.Ee]+)\s*,\s*([-+0-9.Ee]+)\s*,\s*([-+0-9.Ee]+)\s*\)\s*\)",
    re.S,
)


COMPONENTS = {
    "Board": {"shape_rep": 56, "placement": None},
    "SW13": {"shape_rep": 202548, "placement": 209109},
    "NRF24_U2": {"shape_rep": 315093, "placement": 337340},
    "USB_TypeC": {"shape_rep": 470862, "placement": 478377},
}

CRITICAL_EXPORT_ROOTS = {
    "SW13": [1, 205694],
    "NRF24_U2": [1, 325888],
    "USB_TypeC": [1, 456031],
}


def read_entities(path: Path) -> dict[int, str]:
    entities: dict[int, str] = {}
    pending = ""
    with path.open("r", encoding="latin-1") as stream:
        for line in stream:
            if not pending:
                if not line.lstrip().startswith("#"):
                    continue
                pending = line.rstrip("\r\n")
            else:
                pending += " " + line.strip()

            while ";" in pending:
                statement, pending = pending.split(";", 1)
                match = ENTITY_RE.match(statement)
                if match:
                    entities[int(match.group(1))] = match.group(2).strip()
                pending = pending.lstrip()
                if pending and not pending.startswith("#"):
                    pending = ""
    return entities


def entity_type(rhs: str) -> str:
    rhs = rhs.lstrip()
    if rhs.startswith("("):
        return "COMPLEX"
    match = re.match(r"([A-Z0-9_]+)", rhs)
    return match.group(1) if match else ""


def refs(rhs: str) -> list[int]:
    return [int(value) for value in REF_RE.findall(rhs)]


def reachable(entities: dict[int, str], roots: list[int]) -> set[int]:
    seen: set[int] = set()
    stack = list(roots)
    while stack:
        current = stack.pop()
        if current in seen or current not in entities:
            continue
        seen.add(current)
        stack.extend(refs(entities[current]))
    return seen


def representation_solids(entities: dict[int, str], representation: int) -> list[int]:
    rhs = entities[representation]
    candidates = refs(rhs)
    return [
        item
        for item in candidates
        if item in entities
        and entity_type(entities[item])
        in {"MANIFOLD_SOLID_BREP", "BREP_WITH_VOIDS", "FACETED_BREP"}
    ]


def cartesian_point(entities: dict[int, str], entity_id: int) -> tuple[float, float, float]:
    match = XYZ_RE.search(entities[entity_id])
    if not match:
        raise ValueError(f"Entity #{entity_id} is not a 3D CARTESIAN_POINT")
    return tuple(float(match.group(i)) for i in range(1, 4))


def direction(entities: dict[int, str], entity_id: int) -> tuple[float, float, float]:
    rhs = entities[entity_id]
    match = re.search(
        r"DIRECTION\s*\([^,]*,\s*\(\s*([-+0-9.Ee]+)\s*,\s*"
        r"([-+0-9.Ee]+)\s*,\s*([-+0-9.Ee]+)\s*\)\s*\)",
        rhs,
        re.S,
    )
    if not match:
        raise ValueError(f"Entity #{entity_id} is not a 3D DIRECTION")
    return tuple(float(match.group(i)) for i in range(1, 4))


def axis2(entities: dict[int, str], entity_id: int):
    axis_refs = refs(entities[entity_id])
    origin = cartesian_point(entities, axis_refs[0])
    z_axis = direction(entities, axis_refs[1])
    x_axis = direction(entities, axis_refs[2])
    y_axis = (
        z_axis[1] * x_axis[2] - z_axis[2] * x_axis[1],
        z_axis[2] * x_axis[0] - z_axis[0] * x_axis[2],
        z_axis[0] * x_axis[1] - z_axis[1] * x_axis[0],
    )
    return origin, x_axis, y_axis, z_axis


def transform(point, placement):
    if placement is None:
        return point
    origin, x_axis, y_axis, z_axis = placement
    return tuple(
        origin[i]
        + point[0] * x_axis[i]
        + point[1] * y_axis[i]
        + point[2] * z_axis[i]
        for i in range(3)
    )


def bounds(points):
    pts = list(points)
    if not pts:
        raise ValueError("No points found")
    low = [min(point[i] for point in pts) for i in range(3)]
    high = [max(point[i] for point in pts) for i in range(3)]
    return {
        "min": [round(value, 6) for value in low],
        "max": [round(value, 6) for value in high],
        "size": [round(high[i] - low[i], 6) for i in range(3)],
        "center": [round((high[i] + low[i]) / 2, 6) for i in range(3)],
    }


def physical_vertex_points(entities: dict[int, str], solid_roots: list[int]):
    graph = reachable(entities, solid_roots)
    point_ids: set[int] = set()
    for item in graph:
        rhs = entities[item]
        if entity_type(rhs) == "VERTEX_POINT":
            point_ids.update(refs(rhs))
    points = [cartesian_point(entities, item) for item in point_ids]
    return points, graph


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("step", type=Path)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--board-step", type=Path)
    parser.add_argument("--critical-step-dir", type=Path)
    args = parser.parse_args()

    entities = read_entities(args.step)
    report = {
        "source": str(args.step.resolve()),
        "entity_count": len(entities),
        "components": {},
    }

    for name, configuration in COMPONENTS.items():
        solid_roots = representation_solids(entities, configuration["shape_rep"])
        local_points, graph = physical_vertex_points(entities, solid_roots)
        placement = (
            axis2(entities, configuration["placement"])
            if configuration["placement"] is not None
            else None
        )
        world_points = [transform(point, placement) for point in local_points]
        solids = []
        for solid_root in solid_roots:
            solid_points, solid_graph = physical_vertex_points(entities, [solid_root])
            solids.append(
                {
                    "entity": f"#{solid_root}",
                    "topology_entity_count": len(solid_graph),
                    "vertex_count": len(solid_points),
                    "local_bounds_mm": bounds(solid_points),
                    "pcb_bounds_mm": bounds(
                        transform(point, placement) for point in solid_points
                    ),
                }
            )
        report["components"][name] = {
            "shape_representation": f"#{configuration['shape_rep']}",
            "solid_count": len(solid_roots),
            "topology_entity_count": len(graph),
            "vertex_count": len(local_points),
            "placement": None
            if placement is None
            else {
                "origin": [round(value, 6) for value in placement[0]],
                "x_axis": [round(value, 9) for value in placement[1]],
                "y_axis": [round(value, 9) for value in placement[2]],
                "z_axis": [round(value, 9) for value in placement[3]],
            },
            "local_bounds_mm": bounds(local_points),
            "pcb_bounds_mm": bounds(world_points),
            "solids": solids,
        }

    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    print(rendered)
    if args.json:
        args.json.write_text(rendered + "\n", encoding="utf-8")
    def write_subset(output_path: Path, selected_entities: set[int], description: str) -> None:
        header = """ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('DESCRIPTION_TOKEN'),'2;1');
FILE_NAME('PCB1_2026-08-10_Subset.step','2026-08-10T00:00:00',('Codex'),(''),
'Open CASCADE STEP processor 7.8','Open CASCADE 7.8','');
FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));
ENDSEC;
DATA;
""".replace("DESCRIPTION_TOKEN", description)
        with output_path.open("w", encoding="latin-1", newline="\n") as output:
            output.write(header)
            for entity_id in sorted(selected_entities):
                output.write(f"#{entity_id} = {entities[entity_id]};\n")
            output.write("ENDSEC;\nEND-ISO-10303-21;\n")

    if args.board_step:
        write_subset(
            args.board_step,
            reachable(entities, [1, 49]),
            "Board extracted from PCB1_2026-08-10.step",
        )
    if args.critical_step_dir:
        args.critical_step_dir.mkdir(parents=True, exist_ok=True)
        for name, roots in CRITICAL_EXPORT_ROOTS.items():
            write_subset(
                args.critical_step_dir / f"PCB1_2026-08-10_{name}.step",
                reachable(entities, roots),
                f"{name} extracted from PCB1_2026-08-10.step",
            )


if __name__ == "__main__":
    main()
