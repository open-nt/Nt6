# Nt digital twin engine survey

A survey of what Nt would need to serve as the engine for digital twins: a 3D scene that mirrors
the live state of a real plant, vehicle, building or site, and that can replay that state and
simulate it. It records what already exists at 6.0.5 (main, 2026-11-18), identifies the gaps and
proposes modules and phases. No code was changed for this survey.

This file lives in `doc/internal/` and is Markdown, so ndoc does not publish it.

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
|---|---|
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
|---|---|---|
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

```
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
