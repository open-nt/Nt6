#!/usr/bin/env python3
# MIT License - see LICENSE
"""Compares the species of ntgentree with the Tallo tree allometry database.

Tallo (Jucker et al. 2022, https://doi.org/10.5281/zenodo.6637599, CC BY 4.0) holds stem diameter, height and crown
radius for half a million trees. For each tree species of ntgentree this prints, from the records of the species (or
of its genus when there are few), what a mature tree looks like:

  height     50th and 90th percentile, in metres, of the mature trees: those at least half as tall as the 90th
             percentile of the species. heightMin and heightMax of a species take these.
  trunk      25th, 50th and 75th percentile of (stem radius / height) of those trees: trunk_radius_ratio
  crown      25th, 50th, 75th and 90th percentile of (crown radius / height): crown_radius

Records are taken from the region of the species first (the British Isles or Australia, then Europe for UK species),
since a river red gum planted in Spain is not the tree of the Murray, and from all over the world only if the region
has too few.

Inventories are mostly of trees in woods, which have narrower crowns than a tree that stands alone; ntgentree grows
specimen trees, so it takes the 75th percentile of the trunk ratio and the 90th of the crown ratio (the medians are in
the output for forest-grown trees).

A tree grown with the default parameters is checked against these, and --json writes the numbers for use in
species.h. The database is not part of the repository; download Tallo.csv from the address above.

    calibrate_tallo.py --tallo Tallo.csv [--tool ntgentree] [--json out.json]
"""

import argparse
import csv
import json
import statistics
import subprocess
import sys
from collections import defaultdict


def percentile(values, p):
    values = sorted(values)
    k = (len(values) - 1) * p
    lo = int(k)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (k - lo)


# Bounding boxes: latitude low, high, longitude low, high.
REGIONS = {
    "uk": [("British Isles", (49.5, 61.0, -11.0, 2.0)), ("Europe", (35.0, 72.0, -12.0, 40.0))],
    "au": [("Australia", (-44.0, -10.0, 112.0, 154.0))],
}


def tiers(region, lat, lon):
    """The names of the regions that contain the point, as keys, with 'world' for all."""
    found = ["world"]
    for name, (la0, la1, lo0, lo1) in REGIONS.get(region, []):
        if la0 <= lat <= la1 and lo0 <= lon <= lo1:
            found.append(name)
    return found


def read_tallo(path, wanted):
    """wanted maps a species name to its region. Returns {(species or genus, tier): records}."""
    wanted_genera = {name.split()[0]: region for name, region in wanted.items()}
    records = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if row["height_outlier"] == "Y":
                continue
            try:
                lat, lon = float(row["latitude"]), float(row["longitude"])
                h = float(row["height_m"])
                d = float(row["stem_diameter_cm"])
                c = float(row["crown_radius_m"]) if row["crown_radius_outlier"] != "Y" and row["crown_radius_m"] else None
            except ValueError:
                continue
            if h <= 0.5 or d <= 0:
                continue
            record = (h, d / 200.0, c)
            if row["species"] in wanted:
                for tier in tiers(wanted[row["species"]], lat, lon):
                    records[(row["species"], tier)].append(record)
            if row["genus"] in wanted_genera:
                for tier in tiers(wanted_genera[row["genus"]], lat, lon):
                    records[(row["genus"], tier)].append(record)
    return records


def summarise(records):
    heights = [r[0] for r in records]
    top = percentile(heights, 0.9)
    mature = [r for r in records if r[0] >= 0.5 * top]
    crowns = [r[2] / r[0] for r in mature if r[2]]
    mature_heights = [r[0] for r in mature]
    trunks = [r[1] / r[0] for r in mature]
    return {
        "n": len(records),
        "mature": len(mature),
        "height_p50": percentile(mature_heights, 0.5),
        "height_p90": top,
        "trunk_p25": percentile(trunks, 0.25),
        "trunk_p50": percentile(trunks, 0.5),
        "trunk_p75": percentile(trunks, 0.75),
        "crown_p25": percentile(crowns, 0.25) if len(crowns) >= 5 else None,
        "crown_p50": percentile(crowns, 0.5) if len(crowns) >= 5 else None,
        "crown_p75": percentile(crowns, 0.75) if len(crowns) >= 5 else None,
        "crown_p90": percentile(crowns, 0.9) if len(crowns) >= 5 else None,
        "crown_n": len(crowns),
    }


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--tallo", required=True)
    p.add_argument("--tool", default="ntgentree")
    p.add_argument("--json")
    p.add_argument("--min-records", type=int, default=30, help="use the genus below this many records of the species")
    args = p.parse_args()

    out = subprocess.run([args.tool, "--list"], check=True, capture_output=True, text=True).stdout
    ours = []
    for line in out.splitlines():
        parts = line.split(None, 3)
        if len(parts) == 4 and parts[1] in ("tree", "shrub"):
            ours.append((parts[0], parts[3].rsplit("(", 1)[1].rstrip(")"), parts[2]))
    records_by = read_tallo(args.tallo, {name: region for _, name, region in ours})

    result = {}
    print("%-26s %-36s %6s %5s %11s %22s %26s" % ("id", "source", "n", "mat.", "height m", "trunk p25/50/75", "crown p25/50/75/90"))
    for ident, name, region in ours:
        # The species in the region, then in the wider regions, then the world; then the same for the genus.
        records, source = [], name
        names = [n for n, _ in REGIONS.get(region, [])] + ["world"]
        for key, label in ((name, name), (name.split()[0], name.split()[0] + " (genus)")):
            for tier in names:
                found = records_by.get((key, tier), [])
                if len(found) >= args.min_records:
                    records, source = found, "%s, %s" % (label, tier)
                    break
            if records:
                break
        if len(records) < 10:
            print("%-26s %-28s %6d  not enough records" % (ident, name, len(records)))
            continue
        s = summarise(records)
        s["source"] = source
        result[ident] = s
        crown = "%.3f/%.3f/%.3f/%.3f" % (s["crown_p25"], s["crown_p50"], s["crown_p75"], s["crown_p90"]) if s["crown_p50"] else "-"
        print("%-26s %-36s %6d %5d %5.1f/%5.1f %7.4f/%.4f/%.4f %26s" % (
            ident, source, s["n"], s["mature"], s["height_p50"], s["height_p90"],
            s["trunk_p25"], s["trunk_p50"], s["trunk_p75"], crown))
    if args.json:
        with open(args.json, "w") as f:
            json.dump(result, f, indent=2)


if __name__ == "__main__":
    sys.exit(main())
