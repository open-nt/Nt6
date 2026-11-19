# ntmapgen: real-world terrain generator (plan)

This is the plan for `ntmapgen`, a tool that generates a dynamic, geo-referenced outdoor environment
for Nt Twin from real-world data. It is modelled on what GameRealisticMap / ArmaRealMap
(github.com/jetelain/ArmaRealMap) does for Arma 3, but its output is an open, streamable format with
twin metadata instead of an Arma WRP/PBO. It accompanies `digital-twin-survey.md` (§2.1a large
worlds, §2.1b dynamic environments) and `large-outdoor-environments.md` (research).

ntmapgen lives in its own repository (`ntmapgen`, next to Nt) and builds against an installed Nt.
Nt itself has only what the tool needs from it: the geodesy in Nt Twin (`QtTwin::Ellipsoid`,
`LocalFrame`, `TransverseMercator`, `UtmZone`). Phase 1 (terrain) was started on 2026-11-19.

This file lives in `doc/internal/` and is Markdown, so ndoc does not publish it.

## Clean-room rule

The ArmaRealMap repository has **no top-level licence** (GitHub reports none, as of 2026-11-19). Only
some sub-folders have their own LICENSE files. The code is therefore all rights reserved. ntmapgen
may follow its published *concepts* (wiki pages, README, the data-source list), but nobody working on
ntmapgen reads, ports or copies its source code. The Arma export (§4a) is written from the
public format pages of the Bohemia community wiki only, not from ArmaRealMap's code or its
`bis-file-formats` submodule.

## What GameRealisticMap does (from its public docs)

- **Inputs** are downloaded automatically: NASA SRTM elevation, OpenStreetMap, and Sentinel-2
  cloudless imagery (s2maps.eu, EOX). The generated map must credit all three.
- **Projection.** The area is projected to UTM, giving a flat metric terrain grid.
- **Builders.** A graph of one builder per data type: roads, railways, buildings, fences, forests,
  scrub, rocks, lakes, ocean, watercourses, farmland, airports, surfaces, cut lines. A builder
  asks for the data it depends on, and each type is built once and cached.
- **Asset configuration.** OSM-derived area types are mapped to sets of game assets:
  - Natural areas: forest, forest additional, forest edge, forest radial, scrub, scrub additional,
    scrub radial, rocks, rocks additional, watercourse, watercourse additional, watercourse radial.
  - Other groups: fences and walls, buildings, objects, ground textures, roads.
- **Filling algorithms.**
  - *Pure random* fits non-colliding objects at random positions. It is used for rocks, dead trees
    and sparse fill.
  - *Seed based* scatters seeds, then places objects near them, so neighbours tend to be the same
    species. It is used for vegetation.
  - *Radial* placement softens the borders of an area.
- **Per-object parameters:** probability, collision radius, fit radius (keeps the object inside
  the polygon), minimum and maximum Z offset, and minimum and maximum scale. Heading is random.
  Each area picks a density between two bounds.
- **Variations.** Each area picks a variation at random and gets a ground texture with small
  clutter objects.
- **Preview.** A 200 × 200 m preview area is used to tune density, because high densities make
  generation slow.
- **Studio.** A GUI guides the steps and includes an asset browser.

## Goals for ntmapgen

1. **Real positions.** Every generated point has an exact, reversible mapping to WGS84. GPS and
   real-world positioning in Nt Twin (see §6) depend on it.
2. **Dynamic, not baked.**
   - Terrain, instance lists and clutter are kept separate.
   - Every placed tree, building, fence or bridge is an instance of a glTF asset with a feature ID.
   - A stateful object (door, gate, destructible building) becomes an Nt Twin entity.
   - Vegetation carries wind weights. See survey §2.1b.
3. **Deterministic.** The same inputs, configuration and seed produce byte-identical output. Each
   tile is seeded from its tile coordinates, so regenerating one tile does not change its
   neighbours.
4. **Scale.** The tool targets 60 × 60 km at a 1–2 m height grid, with a design that does not
   prevent larger areas. It generates tile by tile, in parallel and resumably.
5. **Open output.** 3D Tiles 1.1 with glTF content, readable by Nt and by Cesium-compatible tools.
6. **Headless.** It is a command-line tool first. A studio GUI comes later, built on the same
   library.

## 1. Inputs

| Data | Default source | Licence / credit | Notes |
| --- | --- | --- | --- |
| Elevation | Copernicus DEM GLO-30 | Free, commercial use OK; credit DLR / Copernicus | Better than SRTM; national 1 m LiDAR (EA, USGS 3DEP) used when available |
| Elevation (defence) | DTED 0/1/2 | Per source | Read via GDAL |
| Imagery | A Sentinel-2 L2A mosaic built by ntmapgen, or a user GeoTIFF | "Contains modified Copernicus Sentinel data [year]" | s2maps.eu cloudless is not the default until its per-year terms are checked |
| Bare-earth DEM | FABDEM, supplied by the customer only | CC BY-NC-SA | Non-commercial, never bundled or fetched by default |
| Vectors | OpenStreetMap (`.osm.pbf` extract) | ODbL | Output tileset is a Derivative Database (see §7) |
| Land cover | ESA WorldCover 10 m | CC BY 4.0 | Fills areas OSM does not tag |
| Buildings | OSM, then Microsoft / Google Open Buildings | ODbL / CC BY 4.0 | Footprints and heights |
| User overlays | GeoJSON, GeoPackage, CityGML, glTF sites | User's | Industrial sites, bases, surveyed plant |

Data is cached under `~/.cache/nt-mapgen/` by source, version and tile. A project file records the
exact versions it used.

Google Photorealistic 3D Tiles are **not** an input. Their terms forbid caching, offline use and
derived models.

## 2. Coordinate frames

- **Generation frame:** a projected, metric CRS. By default this is the UTM zone of the area's
  centre; the user may override it (for example with a national grid such as OSGB36). All
  placement runs here, as in GameRealisticMap, because it is flat and metric.
- **Georeference:** the project stores the CRS (EPSG code or WKT2) and the origin. Conversions use
  PROJ, so generation-frame to WGS84 is exact (to PROJ's accuracy) and reversible.
- **Runtime frame:** ECEF tile transforms in `tileset.json` (doubles), and local tile content in
  float32 around each tile centre. The renderer works relative to the camera.
- **Curvature.** UTM is flat but the Earth is not: across 60 km the ground drops about 280 m.
  ntmapgen bakes curvature into the vertex positions of each tile, not only into tile placement.
  Curving only the placement would leave neighbouring tiles a few centimetres apart at their
  seams. The result keeps the twin's GPS and line-of-sight correct. A flat Arma-style world is
  available as an option (`--flat`) for game-like use, and is labelled non-geodetic. The generation frame keeps its UTM scale factor (0.9996 to about 1.0010 across a
  zone), and the project records it, so ranges are reported correctly.

## 3. Pipeline

ntmapgen is a graph of builder stages. Each stage produces one typed dataset, is cached on disk,
and declares the datasets it depends on. This follows the same idea as GameRealisticMap's builders,
but is independently designed.

1. **Fetch.** Download or read the elevation, imagery, OSM, land cover and building sources for
   the area plus a margin.
2. **Project.** Reproject everything into the generation frame. Rasters go through GDAL warp;
   vectors through PROJ.
3. **Classify features.** A tag-rule table in the configuration turns OSM tags into typed
   features: roads (by class, width, surface, lanes), railways, bridges, tunnels, buildings (by
   use and levels), fences and walls, power lines, forests, scrub, rocks, water bodies,
   watercourses, farmland, airfields and industrial areas. WorldCover fills untagged land.
4. **Terrain conditioning.**
   - Fuse DEM sources: LiDAR where it exists, Copernicus elsewhere, blended at the seams.
   - Correct surface models to bare earth. Copernicus DEM is a surface model that includes
     canopy and buildings. Where a forest or building footprint exists, estimate the ground
     underneath it, so placed trees and buildings do not float on the canopy.
   - Flatten road and rail corridors and add embankments or cuttings with configurable slopes.
   - Flatten building platforms and level water surfaces.
   - Carve watercourse beds.
   - Add optional procedural detail below the source resolution (seeded fractal, damped on roads
     and water).
5. **Surface mask.** Rasterise a per-texel material ID (grass, dirt, rock, tarmac, gravel, sand,
   field, mud and so on) from the classified features, land cover and slope. This drives ground
   materials, procedural clutter at runtime, vehicle physics (friction) and sensor materials.
6. **Linear features.** Build road, rail and track meshes on splines conformed to the terrain,
   with junctions, markings as decals, bridges from an asset library, and fences and walls as
   segmented assets along ways.
7. **Buildings.**
   - Match each footprint to a library asset by type, size and aspect ratio, rotated to the
     footprint's main axis.
   - If nothing fits within tolerance, extrude a procedural building from the footprint, levels
     and roof tags.
   - Landmarks may be replaced by hand-made assets through a per-OSM-ID override.
8. **Natural areas.** Fill each polygon with the per-category sets from the asset configuration,
   using *pure random*, *seed based* or *radial* placement:
   - density range, probability, collision radius and fit radius
   - Z offset and scale range
   - random heading
   - slope and altitude limits (an addition over GameRealisticMap)

   Placement uses a spatial hash in each tile, and tiles overlap by the largest collision radius
   so objects do not collide across seams.
9. **Twin entities.**
   - Every instance gets a feature ID.
   - A stateful asset type creates an Nt Twin entity record. Stateful types are those whose asset
     declares hinged parts, variants such as intact or destroyed, or a type the twin binds (power
     line, valve, gate).
   - The record holds the asset type, OSM ID, position in WGS84 and in the generation frame, and
     the initial state.
10. **Tile and encode.**
    - Build a 3D Tiles implicit quadtree with geometric error.
    - Terrain is a 16-bit heightmap pyramid, quantized per tile. Nt renders it CDLOD-style.
      Each tile also gets a regular-grid glTF mesh with skirts, so other 3D Tiles clients can
      show and validate the world. Quantized-mesh is an import format only.
    - OSM-derived content goes in tilesets of its own, with the source recorded for each
      feature, so ODbL share-alike stays confined to them.
    - Instance tiles use `EXT_mesh_gpu_instancing` and refer to shared asset glTFs.
    - Metadata uses `EXT_mesh_features` and `EXT_structural_metadata`.
    - Imagery and the surface mask are KTX2 / Basis; meshes use meshoptimizer.
    - Each tile has its own seed, so tiles are generated in parallel.
11. **Package.**
    - `tileset.json` and the tiles.
    - Shared assets.
    - `twin-entities.cbor`.
    - `collision/`: static bodies for Jolt, used headless by Nt Sim. Heightfield collision
      reads the main height pyramid; there is no second copy of it.
    - `georef.json`: CRS, origin, ECEF transform, scale factor, curvature mode, and a geoid
      grid for each tile, so Nt Twin can convert positions without PROJ.
    - `ATTRIBUTION.md` with the credits every source requires.
    - `manifest.json`: data versions, configuration hash, seed and tool version.

## 4. Asset configuration

A JSON file (QJsonDocument) of the same kind as GameRealisticMap's asset configuration, but its
assets are glTF files and it carries twin metadata.

```json
{
  "version": 1,
  "assets": {
    "tree.pine.tall": {
      "gltf": "assets/vegetation/pine_tall.glb",
      "kind": "tree",
      "radius": 2.5,
      "wind": "vertex-attribute",
      "variants": { "intact": "Pine", "felled": "PineFelled" },
      "proxies": { "collision": "PineCollision", "los": "PineCrown",
                   "ballistic": "PineTrunk", "sensor": "PineCrown" },
      "sensorMaterial": "vegetation.conifer"
    }
  },
  "areas": {
    "forest": {
      "variations": [{
        "density": [0.04, 0.07],
        "main": { "algorithm": "seed", "objects": [
          { "asset": "tree.pine.tall", "probability": 0.7, "collision": 2.0, "fit": 0.0,
            "scale": [0.85, 1.15], "slopeMax": 35 }
        ]},
        "additional": { "algorithm": "random", "objects": [] },
        "edge": { "algorithm": "seed", "objects": [] },
        "radial": { "algorithm": "seed", "width": 12, "objects": [] },
        "surface": "forest_floor"
      }]
    }
  },
  "buildings": { "residential": [], "industrial": [], "agricultural": [] },
  "roads": { "primary": { "width": 7.5, "surface": "tarmac", "embankment": 1.5 } },
  "fences": {},
  "surfaces": { "forest_floor": { "material": "materials/forest_floor.json", "clutter": [] } },
  "osmRules": "rules/osm-default.json"
}
```

The asset rules in survey §2.1b apply to assets:

- wind weights in `_WIND` or `COLOR_0`
- hinged parts as nodes
- variants as alternative nodes
- collision, line-of-sight and sensor proxies as separate meshes

ntmapgen checks each asset at load: bounds, the declared variants exist, the proxies exist, and the
units are metres.

**Assets come from a real library that the user supplies (decided 2026-11-19).** It is not bundled
with Nt or committed to the repository.

- **Library setup.** The project file names the library's root directory. `ntmapgen assets scan`
  indexes the library into `assets.json` and fills in values from each glTF where it can: bounds,
  radius, the kind guessed from node and material names, and the variants and proxies that exist.
  The user then confirms or corrects the results, as GameRealisticMap's asset browser types
  objects automatically. It also reports what is missing: wind weights, proxies, sensor materials.
- **Starting configuration.** Instead of a bundled starter set, `ntmapgen init --assets <library>`
  writes a starting asset configuration for that library, which the user then edits. It maps
  library assets to area categories by kind.
- **Non-glTF assets.** If the library is not glTF, a one-off offline conversion step comes before
  `scan`. The conversion keeps the asset IDs stable, so configurations survive a re-export.
- **Tests.** Nt's own tests use a few tiny glTF assets that the tests generate. They exercise the
  pipeline only and are never offered as content.
- **Output.** A packaged world refers to library assets by ID and version. `package` can copy the
  assets it uses into the output, or leave them as references when the library is deployed
  separately. Which is allowed depends on the library's licence.

## 4a. Export to Arma 3 and VBS (proposed 2026-11-19: an export target, not the default)

The user asked whether ntmapgen can export to Arma 3 and VBS, and whether the Bohemia formats
(PBO, PAA, rapified config, WRP, P3D) could be the default format of the twin. They can be an
export target. They should not be the default, for these reasons:

- **Flat world.** A WRP terrain is one flat heightfield grid with object positions in 32-bit
  floats, so it cannot hold the curved world the twin uses (decided 2026-11-19). GNSS positions,
  line of sight and ranges over tens of kilometres would be wrong by the curvature: about 70 m of
  drop at 30 km.
- **No streaming.** A WRP is a single file loaded whole. There are no levels of detail by tile and
  no feature IDs or metadata, so no link to Nt Twin entities.
- **Closed parts.** The binarised P3D (ODOL), which the game needs for models, is not publicly
  documented; only the editable MLOD form is. A full Arma build therefore still runs Bohemia's
  own tools (Binarize, through Terrain Builder or pboProject).
- **Lock-in.** The twin would depend on a game's formats, versions and EULA.

The export reads the same stage caches as the 3D Tiles output, which fits because ntmapgen already
generates on a flat UTM grid, as Arma terrains are:

| Stage cache | Arma 3 / VBS3 output |
| --- | --- |
| `cache/height-utm.tif` | Heightmap (ASC/XYZ for Terrain Builder, or the elevation grid of a WRP) |
| `cache/surface-utm.tif` | Surface mask and `layers.cfg` (surface material to Arma surface class) |
| Imagery | Satellite image tiles |
| Instance lists | Object placements, using Arma model paths from the asset configuration |
| Project | `config.cpp` with `CfgWorlds` and `CfgWorldList`, rapified, packed in a PBO |

**Assets.** glTF assets cannot become game models without Bohemia's tools. Instead, the asset
configuration gets a target column: each asset ID maps to a glTF for the twin and to an existing
Arma 3 model path (`a3\plants_f\...`) or a mod's path for the export. This mirrors how
GameRealisticMap maps area categories to the game's own objects.

**Steps:**

1. **Terrain Builder project (lowest risk; started 2026-11-19 as `ntmapgen export arma3`).** Write the heightmap (ASC), satellite and mask PNG
   tiles, `layers.cfg`, and object lists in Terrain Builder's text import format, all of which are
   documented on the wiki. The user imports them into Terrain Builder, which does the
   binarisation. Most of the value, and no undocumented formats.
2. **Direct packaging.**
   - A PBO packer.
   - A rapified `config.cpp` writer.
   - A PAA writer (DXT1/DXT5 with the LZO compression the wiki describes).
   - An OPRW WRP writer.

   With these an unbinarised development terrain loads without Terrain Builder. Check the WRP
   writer against a terrain that Terrain Builder exported.
3. **VBS.** VBS3 uses the same Real Virtuality formats as Arma, with differences to confirm
   against VBS documentation. VBS4 and VBS Blue IG build their terrain through VBS World Server
   from standard sources. Check before writing any VBS-specific code; the 3D Tiles output may
   already be the better route into VBS4.

The export is flat by nature. Its georeference (UTM zone and origin, written into the world
config and a sidecar) maps positions back to the twin, but it inherits Arma's flat world.

## 5. Command line and studio

```text
ntmapgen init  --area 51.20,-1.90,51.74,-1.03 --name salisbury  project/
ntmapgen fetch project/
ntmapgen build project/ [--stage terrain|surface|roads|buildings|nature|tiles] [--tile x,y] [-j N]
ntmapgen preview project/ --at 51.31,-1.71 --size 200     # 200 x 200 m quick check
ntmapgen package project/ out/
```

- Stages can be re-run individually; unchanged inputs are taken from the cache.
- `preview` writes a single-tile glTF that ntgltfview opens directly.
- **Studio (started 2026-11-19).** `ntmapgen-studio` is an Nt Widgets application on the same
  library. It has no moc dependency: it uses lambdas and callbacks only.
  - **Done:**
    - New and open project.
    - Stage status, with background builds and the Arma 3 export.
    - A map with relief, surface, footprints and objects, a cursor read-out, and pan and zoom.
    - An asset browser with the library thumbnails, and an inspector.
    - Undoable hand edits (select, move, turn, rescale, swap asset, delete, add, clear an area),
      saved to `edits.json`.
    - A 3D preview of the square around the view's centre, ray traced in ntgltfview (Vulkan KHR
      ray tracing, DLSS selectable on NVIDIA), as a window or as a still in the studio.
    - It runs headless (`--screenshot`, offscreen platform), and `tests/test_studio.cpp` drives it
      with mouse and keyboard events.
  - **Edits model.** `edits.json` holds removed and changed feature IDs, added objects (IDs with bit
    52 set) and clearing polygons. The edits are applied when objects are read (`readMapObjects`),
    never baked into the stage output, so they survive rebuilds. The `arma3` fingerprint includes
    them.
  - **Preview.** `ntmapgen preview` and the studio share `writePreviewScene()`, which writes one GLB
    of the terrain, its surface texture and the library models, each merged once. The decimated
    `library/preview/models` are used by default: 7.5 MB against 161 MB for 300 objects.
  - **Next:**
    - Embed the ray traced view in the studio. ntgltfview's renderer becomes a library behind a
      QVulkanWindow, through `createWindowContainer`.
    - Live re-preview on edit.
    - A project settings and asset configuration editor.
    - Surface paint edits, and road and fence editing once those stages exist.

## 6. GPS and real-world positioning

Nt Twin models GNSS, so the generated world and the twin share one geodetic truth:

- **Truth.** Each entity's pose is kept in ECEF doubles in Nt Twin. `georef.json` maps it to the
  generation frame and to rendering. Nt Twin converts between WGS84, ECEF and ENU in its own code,
  because it depends only on Nt Core; PROJ is used only inside ntmapgen.
- **Heights.** GNSS reports ellipsoidal height, but DEMs and maps use orthometric height (EGM2008
  for Copernicus). ntmapgen records each source's vertical datum and writes the geoid grid into
  `georef.json`, and the twin converts with that grid. Without this, positions are off by tens of metres vertically.
- **Simulated receiver** (Nt Sim):
  - Takes the entity's true pose and adds an error model: a bias random walk, white noise, and
    accuracy scaled by dilution of precision from a satellite constellation (a GPS / Galileo
    almanac or ephemeris for the simulated time). It also models multipath and blocked satellites
    in urban canyons and under forest, using ray queries against line-of-sight proxies. Finally it
    models denied areas (jamming and spoofing zones as twin entities), which matters for defence
    scenarios.
  - Output is NMEA 0183 (GGA, RMC, GSA, GSV) over a QIODevice, which can be a virtual or real
    serial port through Nt SerialPort, or over UDP/TCP. Real vehicle software can then consume it
    unchanged.
- **Real receivers.** A live vehicle's GNSS updates its twin entity. Use Nt Positioning, a
  Qt-compatible QGeoPositionInfoSource with a QNmeaPositionInfoSource, so both directions use Qt's
  API.
- **Time.** The simulated clock carries GPS time. Converting to UTC applies leap seconds, and
  NMEA output and recorded history both use them.
- **Grids.** Military users want MGRS and UTM strings. GeographicLib (MIT) provides MGRS, and PROJ
  provides UTM.

## 7. Licences and dependencies

- **Build dependencies of ntmapgen only.** Nt libraries do not take them:
  - GDAL (MIT/X) and PROJ (MIT), through `NT_USE_GDAL` / `NT_USE_PROJ`. The tool is skipped when
    either is missing.
  - meshoptimizer (MIT).
  - KTX-Software / Basis Universal (Apache-2.0).
  - GeographicLib (MIT).
  - An OSM PBF reader: libosmium (BSL-1.0), or an own PBF parser on Nt's zlib.
- **Output.**
  - An OSM-derived tileset is a Derivative Database under ODbL and must be offered under ODbL.
    Rendered images and videos are Produced Works, which need attribution only.
  - `ATTRIBUTION.md` lists every source's credit line, as GameRealisticMap requires for its maps.
- **Clean room.** No code from GameRealisticMap / ArmaRealMap. See the rule above.

## 8. Phases

1. **Terrain only.**
   - `init`, `fetch` (Copernicus DEM and a user GeoTIFF), project, and tile terrain into
     3D Tiles.
   - Write `georef.json` and check WGS84 round trips against PROJ in tests.
   - ntgltfview gains a 3D Tiles terrain mode with camera-relative double transforms. This
     depends on the double-precision transform in Nt glTF; see survey §2.1a.
2. **Surface and imagery.** WorldCover and OSM classification, surface mask, KTX2 imagery and
   ground materials.
3. **Natural areas.** The asset configuration plus seed, random and radial filling. Output
   instance tiles with feature IDs and a 200 m `preview`; wind-animated trees in the viewer.
4. **Man-made features.** Roads and railways with terrain conditioning, fences, building footprint
   matching with procedural fallback, and bridges.
5. **Twin integration.** `twin-entities.cbor`, collision export for Jolt, and Nt Twin loading a
   generated world headless. This phase includes the GNSS model and NMEA output from §6.
6. **Studio and scale.**
   - The studio GUI: started (see section 5); embedded ray traced view and configuration editor next.
   - Generation that is incremental, parallel and resumable for 60 km and larger.
   - National LiDAR, Open Buildings, and CityGML and site overlays.

## 9. Open questions

- What is the reference area for the first tests? It decides the UTM zone, the LiDAR source and
  which library assets the first configuration needs.
- What format and licence is the supplied asset library? This decides whether a conversion step is
  needed and whether `package` may copy assets into the output.
- Resolved (2026-11-19): assets come from a real library that the user supplies (§4); there is no
  bundled or placeholder set.
- Resolved (2026-11-19): the world is curved by default, because it is more realistic. `--flat`
  stays as a non-geodetic option.
- Resolved: terrain tiles are heightmap patches, not quantized-mesh. Heightmaps can be edited at
  runtime, the same tiles serve as Jolt collision, and other 3D Tiles tools can still read the
  output. See `large-outdoor-environments.md`.
