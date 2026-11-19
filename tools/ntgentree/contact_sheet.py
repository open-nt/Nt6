#!/usr/bin/env python3
# MIT License - see LICENSE
"""Draws a contact sheet of every plant that ntgentree can grow.

Each species is grown with ntgentree, read back from the glb and drawn as a lit, flat-shaded silhouette from a
three-quarter view, scaled to its own cell (the label gives the height). When textures are used, a surface with a
texture is drawn in the average colour of the image, and swatches of the bark and leaf images are shown.

Needs NumPy, Matplotlib and Pillow. Example:

    contact_sheet.py --tool build/ntgentree --out sheet.png [--textures DIR] [--seed 2]
"""

import argparse
import io
import json
import math
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
from PIL import Image

KINDS = [("tree", "Trees"), ("shrub", "Shrubs and scrub"), ("hedge", "Hedges (one 4 m segment)"),
         ("grass", "Grasses"), ("weed", "Weeds")]
COLUMNS = 6


def load_glb(path):
    data = open(path, "rb").read()
    json_length = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + json_length])
    return doc, data[20 + json_length + 8:]


def accessor(doc, binary, index):
    a = doc["accessors"][index]
    view = doc["bufferViews"][a["bufferView"]]
    dtype = {5126: "<f4", 5125: "<u4", 5121: "u1"}[a["componentType"]]
    n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    arr = np.frombuffer(binary, dtype=dtype, count=a["count"] * n, offset=view.get("byteOffset", 0))
    return arr.reshape(-1, n) if n > 1 else arr


def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(np.asarray(c, dtype=np.float64), 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def image_of(doc, binary, material):
    tex = material["pbrMetallicRoughness"].get("baseColorTexture")
    if not tex:
        return None
    source = doc["textures"][tex["index"]]["source"]
    view = doc["bufferViews"][doc["images"][source]["bufferView"]]
    offset = view.get("byteOffset", 0)
    return Image.open(io.BytesIO(binary[offset:offset + view["byteLength"]])).convert("RGBA")


def material_colours(doc, binary):
    """Linear colour of each material, and the images that are used as textures."""
    colours, images = [], {}
    for i, m in enumerate(doc["materials"]):
        img = image_of(doc, binary, m)
        if img is not None:
            px = np.asarray(img, dtype=np.float64) / 255.0
            w = px[:, :, 3:4]
            mean = (px[:, :, :3] * w).sum(axis=(0, 1)) / max(w.sum(), 1e-6)
            colours.append(srgb_to_linear(mean))
            images[m["name"]] = img
        else:
            colours.append(np.array(m["pbrMetallicRoughness"]["baseColorFactor"][:3]))
    return colours, images


def triangles(doc, binary, mesh_name, colours, yaw):
    mesh = [m for m in doc["meshes"] if m["name"] == mesh_name][0]
    c, s = math.cos(yaw), math.sin(yaw)
    light = np.array([-0.4, 0.8, 0.5])
    light /= np.linalg.norm(light)
    polys, cols, depth = [], [], []
    for prim in mesh["primitives"]:
        if prim["material"] == 2:  # the proxy material
            continue
        pos = accessor(doc, binary, prim["attributes"]["POSITION"])
        tint = accessor(doc, binary, prim["attributes"]["COLOR_0"]).astype(np.float64)[:, :3] / 255.0
        idx = accessor(doc, binary, prim["indices"]).reshape(-1, 3)
        tri = pos[idx]
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        shade = 0.55 + 0.45 * np.abs(n @ light)
        lin = colours[prim["material"]][None, :] * tint[idx].mean(axis=1) * shade[:, None]
        rgb = linear_to_srgb(lin * 1.25)
        x = tri[:, :, 0] * c + tri[:, :, 2] * s
        z = -tri[:, :, 0] * s + tri[:, :, 2] * c
        polys.append(np.stack([x, tri[:, :, 1]], axis=2))
        cols.append(rgb)
        depth.append(z.mean(axis=1))
    polys, cols, depth = np.concatenate(polys), np.concatenate(cols), np.concatenate(depth)
    order = np.argsort(depth)
    return polys[order], cols[order]


def species_list(tool):
    out = subprocess.run([tool, "--list"], check=True, capture_output=True, text=True).stdout
    rows = []
    for line in out.splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4:
            rows.append({"id": parts[0], "kind": parts[1], "region": parts[2], "name": parts[3]})
    return rows


def grow(tool, species, path, seed, textures, max_leaves):
    cmd = [tool, species, "--seed", str(seed), "--max-leaves", str(max_leaves), "-o", path]
    if textures:
        cmd += ["--textures", textures]
    subprocess.run(cmd, check=True, capture_output=True)


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--tool", default="ntgentree")
    p.add_argument("--out", required=True)
    p.add_argument("--textures", help="a directory of textures for ntgentree --textures")
    p.add_argument("--seed", type=int, default=2)
    p.add_argument("--max-leaves", type=int, default=5000, help="leaves per tree in the picture")
    p.add_argument("--yaw", type=float, default=30.0, help="degrees")
    p.add_argument("--dpi", type=int, default=110)
    args = p.parse_args()

    species = species_list(args.tool)
    sections = [(title, [s for s in species if s["kind"] == kind]) for kind, title in KINDS]
    sections = [(t, s) for t, s in sections if s]
    rows = sum(math.ceil(len(s) / COLUMNS) for _, s in sections)
    cell_w, cell_h, head = 3.3, 3.6, 0.45
    fig_w = COLUMNS * cell_w
    fig_h = rows * cell_h + len(sections) * head + 0.9
    fig = plt.figure(figsize=(fig_w, fig_h), facecolor="#f4f1ea")
    fig.text(0.012, 1 - 0.35 / fig_h, "Nt: plants that ntgentree can grow", fontsize=22, fontweight="bold",
             color="#0b2e27", va="center")
    fig.text(0.988, 1 - 0.35 / fig_h, "%d species, seed %d%s" % (len(species), args.seed,
             ", with fal.ai textures" if args.textures else ", flat colours"), fontsize=12, color="#555",
             va="center", ha="right")

    y = fig_h - 0.9
    with tempfile.TemporaryDirectory() as tmp:
        for title, group in sections:
            fig.text(0.012, y / fig_h - 0.02 / fig_h, title, fontsize=16, fontweight="bold", color="#13977f", va="top")
            y -= head
            for n, sp in enumerate(group):
                col, row = n % COLUMNS, n // COLUMNS
                x0 = col * cell_w
                y0 = y - (row + 1) * cell_h
                path = os.path.join(tmp, sp["id"] + ".glb")
                grow(args.tool, sp["id"], path, args.seed, args.textures, args.max_leaves)
                doc, binary = load_glb(path)
                nt = doc["scenes"][0]["extras"]["nt"]
                colours, images = material_colours(doc, binary)
                polys, cols = triangles(doc, binary, "LOD0", colours, math.radians(args.yaw))

                ax = fig.add_axes([(x0 + 0.08) / fig_w, (y0 + 0.62) / fig_h, (cell_w - 0.16) / fig_w,
                                   (cell_h - 0.72) / fig_h])
                ax.set_facecolor("#dcebf2" if sp["kind"] in ("tree", "shrub", "hedge") else "#e6efe0")
                ax.add_collection(PolyCollection(polys, facecolors=cols, edgecolors="none", linewidths=0))
                lo, hi = polys.reshape(-1, 2).min(axis=0), polys.reshape(-1, 2).max(axis=0)
                aw, ah = cell_w - 0.16, cell_h - 0.72
                scale = min(aw / max((hi[0] - lo[0]) * 1.1, 0.05), ah * 0.94 / max(hi[1] * 1.04, 0.05))
                cx = (lo[0] + hi[0]) / 2
                ax.set_xlim(cx - aw / 2 / scale, cx + aw / 2 / scale)
                ax.set_ylim(-0.04 * ah / scale, 0.96 * ah / scale)
                ax.axhline(0, color="#6b5b45", linewidth=1)
                ax.set_xticks([])
                ax.set_yticks([])
                for side in ax.spines.values():
                    side.set_color("#c8c2b4")
                label = sp["name"].rsplit(" (", 1)
                fig.text((x0 + 0.1) / fig_w, (y0 + 0.5) / fig_h, label[0], fontsize=11.5, fontweight="bold",
                         color="#0b2e27", va="top")
                fig.text((x0 + 0.1) / fig_w, (y0 + 0.27) / fig_h, label[1].rstrip(")") if len(label) > 1 else "",
                         fontsize=9, style="italic", color="#444", va="top")
                size = "%.1f m" % nt["height"] if nt["height"] >= 1 else "%d cm" % round(nt["height"] * 100)
                fig.text((x0 + cell_w - 0.1) / fig_w, (y0 + 0.5) / fig_h, "%s  %s" % (sp["region"].upper(), size),
                         fontsize=9, color="#555", va="top", ha="right")
                for k, (name, img) in enumerate(images.items()):
                    sw = fig.add_axes([(x0 + cell_w - 0.62 - k * 0.5) / fig_w, (y0 + cell_h - 0.9) / fig_h,
                                       0.4 / fig_w, 0.4 / fig_h])
                    img = img.copy()
                    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
                    sw.imshow(Image.alpha_composite(bg, img))
                    sw.set_xticks([])
                    sw.set_yticks([])
                print("drew", sp["id"], file=sys.stderr)
            y -= math.ceil(len(group) / COLUMNS) * cell_h
    fig.savefig(args.out, dpi=args.dpi)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
