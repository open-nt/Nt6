# Nt digital twin engine survey

A survey of what Nt would need to serve as the engine for digital twins: a 3D scene that mirrors
the live state of a real plant, vehicle, building or site, and that can replay that state and
simulate it. It records what already exists at 6.0.5 (main, 2026-11-18), identifies the gaps and
proposes modules and phases. No code was changed for this survey.

This file lives in `doc/internal/` and is Markdown, so ndoc does not publish it.

## Decisions (2026-11-18)

- **Qt compatibility where Qt has a module.** Nt Quick 3D, Nt Quick 3D Physics, Nt MQTT, Nt OPC UA
  and Nt Positioning follow Qt's API.
- **Twin-specific parts are Nt-only modules** (Nt Twin, Nt Sim), following the ray tracing
  precedent.
- **Nt Twin depends only on Nt Core** and runs headless; rendering integration lives outside it.
- **Environments are dynamic** (2026-11-19). Vegetation moves in the wind, weather and time of
  day change, and buildings, doors and terrain features change state. Static baked meshes, such
  as photogrammetry, are only acceptable as a far-distance backdrop. See §2.1b.

## Summary

- A twin engine differs from a game engine in where state comes from. A game owns its world and
  invents its state. A twin mirrors state from outside (sensors, PLCs, vehicle buses) and must
  replay, query and simulate it faster than real time. Most of the new work is therefore a data and
  time layer, not rendering.
- Rendering is already on the ray tracing roadmap: Vulkan in Nt GUI is done, ntgltfview renders
  glTF scenes (raster by default, `--raytrace`, optional DLSS), and QRhi, Nt Quick and Nt Quick 3D
  are next. A twin engine should build on that path, not start a separate renderer.
- Qt 6 already has modules for much of the I/O and physics: Qt Quick 3D, Qt Quick 3D Physics, Qt
  MQTT, Qt OPC UA and Qt Positioning. These should be Qt-compatible Nt modules, so twin
  applications stay portable to Qt.
- Qt has no equivalent of the twin-specific part: entity store, property bindings, simulation
  clock, historian, co-simulation and sensor simulation. These belong in Nt-only modules, which
  follows the precedent set for ray tracing.
- The core (entities, bindings, clock, historian) should run headless on Nt Core, so it can be
  tested in CI, run on a server faster than real time, and feed a renderer that runs elsewhere.

## 1. What Nt already provides

| Need | Nt today |
| --- | --- |
| Scene format | Nt glTF (`Nt::Gltf`, `QtGltf::Document`) |
| GPU | QVulkanInstance, QVulkanWindow, Vulkan functions generated from vk.xml (core 1.0–1.3), KHR ray tracing tested on lavapipe; QtOpenGL |
| Viewer | `tools/ntgltfview`: glTF, shadows, reflections, transmission, `--raytrace`, optional DLSS |
| Maths | QVector3D, QQuaternion, QMatrix4x4 (all `float`) |
| Scripting and declarative UI | QtQml (QQmlEngine, QJSEngine, QQmlComponent) |
| 2D UI | Nt Widgets, Nt Charts, Nt Svg |
| Input | Nt Gamepad, Nt GUI keyboard and mouse |
| Audio | Nt Multimedia; Nt FM (procedural FM sound effects such as alarms and engines) |
| Plant I/O | Nt SerialPort, Nt Gpio, Nt Network, Nt WebSockets, Nt HttpServer, Nt DBus |
| Persistence | Nt Sql, QJsonDocument, QCborValue, QDataStream, QSettings |
| Concurrency | QThread, QThreadPool, Nt Concurrent, QFuture, QPromise |
| Edge devices | Nt Core and Nt Network on Zephyr, which can publish telemetry from microcontrollers |

## 2. Gaps by layer

### 2.1 Rendering

These items are already planned on the ray tracing roadmap:

- QRhi, a Qt-compatible subset with a Vulkan backend and later an OpenGL backend
- Nt Quick
- Nt Quick 3D: View3D, Model, PrincipledMaterial, lights, cameras and a glTF loader that wraps
  Nt glTF

Twins also need the following:

- **Large-world precision.** `float` positions jitter at about 10 km from the origin. Store world
  transforms as doubles and render relative to the camera, rebasing the origin. QVector3D stays
  `float` for Qt compatibility, so the double types are Nt-only.
- **Streaming and level of detail.** Sites and cities do not fit in GPU memory. OGC 3D Tiles is
  built on glTF, so it extends Nt glTF naturally.
- **Instancing** for thousands of repeated assets (pumps, lights, racks). Qt Quick 3D has
  `instancing`, which is the Qt-compatible way to expose it.
- **Overlays:** per-asset tint and highlight for alarms and selection, heatmaps on surfaces,
  labels anchored in 3D, section planes and X-ray views.
- **Picking that returns an asset ID**, not only a mesh and triangle.

### 2.1a Large worlds (60 km and beyond)

A single glTF file is not a suitable format for a 60 km world. glTF is suitable as the payload of
each tile inside OGC 3D Tiles 1.1, which is how planet-scale datasets such as Cesium World
Terrain and Google Photorealistic 3D Tiles are delivered. The problems with one file:

- **Precision.** Vertex positions are `float32`. At 32–64 km from the origin, one float32 step is
  2⁻⁸ m ≈ 4 mm. Storing positions at that resolution is tolerable, but transforming them on the GPU
  in float makes close-up geometry jitter by centimetres. Each tile must store small local
  coordinates. The tile and node transforms are kept in double, and the renderer computes
  model-view matrices in double on the CPU, relative to the camera, before converting them to
  float.
- **Nt glTF loses precision today.** Node transforms are stored as `QMatrix4x4`, which is
  `float`, so precision is lost at load even though glTF JSON numbers are doubles. Nt glTF needs
  a double transform alongside the Qt-shaped one, for example `transformD` or a `QtGltf::DMatrix`.
  This is Nt-only.
- **Curvature.** Across a 60 km area the Earth drops d²/2R ≈ 280 m at 60 km, or 70 m at the edge
  measured from the centre. A flat local ENU frame is wrong by that much, which breaks line of
  sight, terrain clamping and geo-registration. Tiles should be placed in ECEF, with an ENU frame
  only near the camera.
- **Size.** A 60 × 60 km area is 3,600 km². A 1 m terrain grid alone is 3.6 × 10⁹ samples, so
  the data must be tiled into a quadtree or octree with hierarchical LOD and streamed by screen-space
  error. Core glTF has no LOD, no bounding hierarchy and no streaming.

3D Tiles 1.1 supplies those missing parts:

- `tileset.json` stores double-precision tile transforms, bounding volumes and a geometric
  error for each tile, which drives refinement.
- Tile content is plain glTF or GLB.
- `EXT_mesh_features` and `EXT_structural_metadata` assign a feature ID to each vertex or texel
  and attach property tables. This gives the "picking returns an asset ID" requirement directly,
  and the IDs can key into the Nt Twin entity store.
- Implicit tiling (subtree files) avoids a huge tileset.json for regular terrain.

Nt glTF already reads `KHR_mesh_quantization` and `KHR_texture_basisu`. Large worlds also need:

- `EXT_meshopt_compression`, or `KHR_draco_mesh_compression`
- `EXT_mesh_features`
- `EXT_structural_metadata`
- `EXT_mesh_gpu_instancing` (trees, lamp posts)

Other formats considered:

| Format | Role |
| --- | --- |
| OpenUSD | Authoring and composition; native doubles and layers, but no geospatial streaming. Already planned; convert to 3D Tiles for runtime. |
| OGC CDB | Common in defence simulators; a source to convert from, not a runtime format. |
| Esri I3S | An alternative to 3D Tiles; only if a customer requires it. |
| OpenFlight | Legacy simulation format; import only. |

The recommendation is to use 3D Tiles 1.1 with glTF content as the runtime format for any world
larger than a few kilometres, and to convert other sources to it offline. Plain glTF files remain
the format for individual assets: vehicles, pumps and buildings placed in the world.

§2.1b qualifies this: because environments must be dynamic, the tiles carry terrain and *instance
lists*, not a single baked mesh of everything.

### 2.1b Dynamic environments: how Arma 3 does it

Arma 3 (Bohemia Interactive's Real Virtuality 4 engine) is the closest existing model for large,
dynamic military terrain. Altis covers about 270 km². The world is not one mesh; it has three
layers:

1. **Terrain.** A heightmap grid, rendered with distance-based level of detail. A satellite image
   is tiled over it for distant colour. A surface mask assigns materials (grass, dirt, rock,
   tarmac), and close to the camera these become detail textures.
2. **Objects.** Roughly a million placed instances of shared models: trees, bushes, rocks,
   buildings, walls, power lines. Each model carries several resolution LODs and several special
   LODs: collision geometry, fire geometry, view geometry for line of sight, roadway and shadow
   volume. Objects are drawn out to a configurable object view distance, which is shorter than the
   terrain view distance; fog and haze hide the cut-off.
3. **Clutter.** Grass and small plants are not placed by hand. They are generated around the
   camera from the surface type and disappear at a short distance.

Vegetation and objects are animated without skeletal rigs:

- **Trees, bushes and grass sway in the vertex shader**, driven by the global wind and by
  per-vertex weights in the model: the trunk stays nearly rigid, branches bend and leaves flutter.
  This costs no CPU, works for a million instances and needs no skeleton.
- **Trees fall** as a scripted topple animation when destroyed, and stay down as part of the
  saved state.
- **Buildings** have doors, windows and gates animated through named selections. The model
  configuration gives rigid rotations or translations driven by an animation source. Damage
  replaces parts or swaps the whole model for a ruin model. These are simple hinges, not skinning.
- **Vehicles** use the same named-selection animations for wheels, turrets, suspension and
  hatches, and skinned skeletons only for characters.

The environment also changes with time:

- Time of day, with sun, moon and stars positioned from the terrain's latitude and longitude and
  the date.
- Overcast, rain, fog, lightning and waves, with wind that changes gradually and is synchronised
  across the network.
- Destruction state of buildings, walls and trees.

Terrain shape does not change: Arma 3 has no deformable terrain.

#### What Nt should take from this

- **Separate terrain, instances and clutter in the tiles.** A 3D Tiles tile holds terrain (a
  quantized mesh or heightmap patch) plus instance lists that refer to shared glTF assets
  (`EXT_mesh_gpu_instancing`, with a feature ID for each instance). Each instance can then be an
  Nt Twin entity with state, which a baked photogrammetry mesh cannot be. Photogrammetry stays as
  a backdrop beyond the object view distance.
- **Wind animation in the vertex shader, not glTF skins.** Tree assets carry sway weights in a
  custom vertex attribute, such as `_WIND` (glTF allows application-specific attributes prefixed
  with an underscore), or in `COLOR_0`. The wind vector and gust parameters come from the
  environment state. glTF skins and morph targets are for characters and for the few objects that
  need them.
- **Hinged parts as glTF node animation.** Doors, valves, cranes and turrets are separate glTF
  nodes. Twin properties drive their transforms directly (a valve at 40 % means a rotation of
  36°). Keyframed glTF animations are used where a canned motion exists.
- **Damage and state as model variants.** An asset declares variants (intact, damaged, destroyed,
  open, closed) as alternative nodes or meshes, and an Nt Twin property selects one.
- **Procedural clutter** from a surface-type mask in each terrain tile, generated on the GPU near
  the camera and seeded from the tile coordinates, so the result is the same each run.
- **Special LODs as glTF metadata.** Collision, line-of-sight and sensor proxies are separate
  meshes in the asset, marked through `extras` or a small Nt-only extension, so Nt Sim and sensor
  simulation do not ray-trace render meshes.
- **Environment as twin state.** Time of day, sun position, cloud, rain, fog, wind and sea state
  are properties of an environment entity in Nt Twin. They can be bound to a live weather feed,
  replayed by the historian, or scripted for what-if runs. They then drive lighting, shaders,
  particles and Nt Sim (wind loads on cranes, rain attenuation for lidar), all from one source.
- **Ray tracing and moving geometry.** Swaying vegetation means bottom-level acceleration
  structures change every frame. Refit them instead of rebuilding them, only near the camera.
  Beyond that, use static or alpha-tested proxies for shadows and reflections, and keep a static
  bottom-level structure per variant for hinged parts.
- **Deformable terrain** (craters, earthworks, flooding) is beyond Arma 3. Leave it out of the
  first version. Treat it later as per-tile heightmap edits stored as Nt Twin state.

### 2.2 World model (Nt-only)

- **Entity store.** Each asset has an ID, a type, typed properties (temperature, valve position,
  health, fault state) and an optional binding to a glTF node. A QObject per entity does not
  scale to 10⁴–10⁶ assets. Use a data-oriented store (columns per property type) and expose
  QObject or QML façades only for entities currently in view or selected.
- **Schemas.** Asset types should be defined in data and imported from the existing standards
  where possible: DTDL, the Asset Administration Shell (AAS, IEC 63278) and OPC UA information
  models.
- **Spatial index.** A BVH or octree for range queries ("assets within 50 m") and for picking.
  The ray tracer builds acceleration structures anyway, so both should share one scene
  description.
- **Coordinate reference systems.** Conversions between WGS84, ECEF and local ENU, plus
  geo-referencing a glTF scene. QGeoCoordinate in a Qt-compatible Qt Positioning subset covers the
  public type.

### 2.3 Time (Nt-only)

This layer differs most from a game engine.

- **Simulation clock.** Kept separate from wall-clock time. It can pause, step, scrub and run at
  any rate, and it drives a fixed-step simulation tick that does not depend on the render frame
  rate.
- **Historian.** Every property change is stored with a timestamp, the source and a quality flag,
  so the world can be rebuilt at any instant. Nt Sql is too slow at telemetry rates. Use an
  append-only log of CBOR chunks, with periodic snapshots and a time index.
- **Modes.** Live, replay and what-if (a fork of live state with simulated inputs) use the same
  scene, so the UI and renderer cannot tell which mode is running.
- **Determinism.** A replay, or a simulation from a snapshot, must give the same result each
  time. That rules out wall-clock time and unordered containers in the update path.

### 2.4 Connectivity

| Protocol | Status | Kind |
| --- | --- | --- |
| MQTT | Qt has Qt MQTT | Qt-compatible module |
| OPC UA | Qt has Qt OPC UA | Qt-compatible module, client first |
| DDS | No Qt module; used by GVA (Def Stan 23-009), links to the gva-hmi port | Nt-only |
| Modbus, CAN | Qt has Qt SerialBus | Qt-compatible, later |
| WebSockets and HTTP | Present | Done |

On top of these protocols, a **binding layer** maps topics, nodes and registers to entity
properties. Bindings are declared in JSON or QML, with units, scaling, deadband, quality and
staleness. Data that has stopped updating must look stale in the scene, not stay at its last
value.

### 2.5 Simulation (Nt-only, apart from physics)

- **Physics:** collision, rigid bodies and kinematic chains for robots, cranes and turrets. Qt
  Quick 3D Physics wraps PhysX. Nt should present the same QML API and back it with Jolt (MIT),
  in line with the rule against vendoring.
- **Co-simulation (FMI 2.0/3.0).** This loads FMUs exported from Modelica, Simulink or
  Dymola and steps them on the simulation clock. It is the most valuable twin-specific feature,
  because engineering teams already model their equipment this way.
- **Sensor simulation.** Virtual cameras use the renderer. Lidar and radar use ray queries against
  the acceleration structures from the ray tracing roadmap, so lidar costs little once
  ray tracing exists.
- **Headless.** The whole simulation runs without Nt GUI, in CI, on a server or faster than real
  time, so the world model must not depend on Nt GUI.

### 2.6 Tooling

- **Scene editor** (`tools/ntscene`, following the `nt` prefix rule). It extends ntgltfview with an
  entity tree, a property inspector, a binding editor and a timeline.
- **Import.** glTF exists and OpenUSD is planned. Twins also need IFC (BIM, for buildings) and
  CityGML. CAD should be converted offline (STEP to glTF), not imported.
- **Remote viewing.** Either stream rendered frames over WebSockets, or serve the scene and
  property deltas to a browser client over Nt HttpServer and WebSockets.

## 3. Proposed modules

```text
Qt-compatible:  Nt Quick 3D · Nt Quick 3D Physics · Nt MQTT · Nt OPC UA · Nt Positioning
Nt-only:        Nt Twin   (entities, schemas, bindings, clock, historian, spatial index, CRS)
                Nt Sim    (fixed-step scheduler, FMI, sensor simulation via ray queries)
                Nt DDS, 3D Tiles streaming, double-precision world transforms
```

As with ray tracing, applications written against the Qt-compatible modules still run on Qt.
Twin behaviour is opt-in through the Nt-only modules, and each class's `\differences` block
records where Nt goes beyond Qt.

Nt Twin depends only on Nt Core, so the entity store, clock and historian can also build for
Zephyr if a gateway needs them. Nt Sim depends on Nt Twin. Rendering integration lives in Nt
Quick 3D or the viewer, not in Nt Twin.

## 4. Phases

1. **Headless Nt Twin core.** Entity store, schemas from JSON, simulation clock, historian with
   replay, and Nt MQTT as the first live source. Everything is testable in CI without a GPU.
2. **Viewer integration.** Bind glTF nodes to asset IDs in ntgltfview. Properties drive colour,
   visibility and transforms; add picking, an inspector panel and a timeline scrubber.
3. **More sources.** Nt OPC UA (client), then Nt DDS, with staleness and quality shown in the
   scene.
4. **QML twins.** Once QRhi and Nt Quick 3D land, expose entities to QML so a twin can be written
   as a View3D scene with declarative bindings.
5. **Simulation.** Fixed-step scheduler, FMI co-simulation, then Nt Quick 3D Physics on Jolt.
6. **Scale and sensors.** Double-precision origins, 3D Tiles streaming and instancing; sensor
   simulation once the ray tracing layer on QRhi exists.

## 5. Open questions

- Which domain comes first: vehicles (GVA, DDS) or industrial plant (OPC UA, MQTT)? The answer
  sets the order of phase 3.
- Historian scale: hours of data on one machine, or months on a server? This decides whether an
  embedded log is enough or an external time-series database needs an adapter.
- Which schema format comes first: DTDL, AAS or OPC UA information models?
- Should remote viewing stream frames (simpler; needs a GPU on the server) or the scene (scales to
  many viewers; needs a browser renderer)?
