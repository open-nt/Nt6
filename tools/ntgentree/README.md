# ntgentree

Grows trees, grasses and weeds from rules and a seed and writes them as binary glTF (`.glb`) with levels of
detail, wind weights, proxies and sensor properties. The README of the repository has the overview; this file
has the details. Run `ntgentree --help` for the options.

## Species

`ntgentree --list` prints them. They are a starting set for the UK and Australia, with the habit of a mature
plant; the numbers in `species.h` were chosen to look right, they are not fitted to surveys.

| Kind | UK | Australia |
| --- | --- | --- |
| Trees | English oak, European beech, silver birch, Scots pine, common hawthorn | River red gum, spotted gum, ghost gum, golden wattle, river she-oak |
| Shrubs and scrub | Gorse, hazel, heather, blackthorn | Coast tea-tree, old man saltbush, broom honey-myrtle |
| Hedges | Hawthorn, yew, beech | Coastal rosemary, lilly pilly |
| Grasses | Cocksfoot, perennial ryegrass, tufted hair-grass | Kangaroo grass, common tussock-grass, lobed spinifex, weeping grass |
| Weeds | Dandelion, broad-leaved dock, spear thistle, stinging nettle, bracken, blackberry | Paterson's curse |

Shrubs and hedges are `TreeSpecies` too (`kind` is `"shrub"` or `"hedge"`). A shrub grows `stems` stems from the
ground that spread by `stem_lean` degrees, with a low crown. A hedge is a box `hedge_length` by `hedge_width` metres and
as high as the height: its stems stand in a row about 0.3 m apart and every branch stops at the faces of the box, so
segments can be placed end to end. A hedge's leaf area index is over its footprint.

To add one, add an entry to `makeTreeSpecies()`, `makeGrassSpecies()` or `makeWeedSpecies()` in `species.h`
(including the descriptions that become texture prompts); `tests/test_gentree.cpp` then grows it with the rest.

## How a tree is grown

1. **Trunk.** A tapering, slightly wobbling path with a flare at the ground. `leader_end` is where it stops: 1 gives
   an excurrent tree (a pine), less gives a tree that forks into limbs (an oak or a gum).
2. **Branches, as a competition for space** (`competition`, the default for trees; after Palubicki et al. 2009).
   Attraction points fill the crown (`fill` points for each cubic metre, denser near its surface, in a crown that is
   lopsided by `asym`). Every shoot has buds, and each point is seen by its nearest bud. A branch's supply is the
   number of points its buds see; it is passed down from the base and shared at each fork between the main axis and
   the side branch, weighted by `apical` for the axis (Borchert and Honda 1984). A bud with enough supply grows
   metamers towards the points it sees, and points are used up when a branch comes near, so shaded branches stop.
   Side branches that find no space for `shed` iterations die. The form comes from three numbers: `apical` above
   0.5 gives a trunk that carries on to the top (a pine), below it a trunk that forks in the crown (an oak or a
   gum); `shed` sets how bare the lower crown is; `fill` and `asym` set how full and how lopsided it is.
   `--param competition=0` uses the older model instead: limbs and branches of fixed counts, angles and lengths on
   the trunk (Weber and Penn 1995), with twigs that fill the crown by space colonization (Runions et al. 2007);
   `fill=0` as well draws the twigs at random. Shrubs and hedges use that model.
   Branch radii follow the pipe model (Shinozaki et al. 1964), so limbs swell where they take on branches.
3. **Leaves.** Leaf area is `lai × density ×` the projected crown area. Leaves sit on the last order of branches,
   mostly towards the tips, droop by `leaf_droop`, and are shaded darker towards the inside of the crown. If that
   many leaves exceed `--max-leaves`, there are fewer and each is larger, so the area stays.
4. **Levels of detail.** The skeleton is meshed four times (see the README).

Wind weights `_WIND` = (trunk bend, branch flex, leaf flutter, phase), stored as normalised bytes. Leaves have
flutter 1; the branch flex weight grows with the order of the branch and along it.

## Parameters of `--param`

`--list-params` prints them. `height_min`, `height_max`, `trunk_radius_ratio` (trunk radius over height), `flare`,
`leader_end`, `crown_base` and `crown_radius` (fractions of the height), `profile` (crown shape exponent), `wobble`
and `lean` and `stem_lean` (degrees), `stems`, `hedge_length` and `hedge_width` (metres), `leaf_length` (metres), `leaf_aspect`, `leaf_droop` and `lai`. For each order of branch 1 to 3:
`count`, `angle`, `curve`, `tropism` and `radius`, and `length` for orders 2 and 3: `angle1=45` is the angle of the
limbs from the trunk. A description such as "mature river red gum, drought-stressed, sparse crown" becomes
`eucalyptus_camaldulensis --density 0.5`, and a program (or an LLM) can fill in the options in the same way.

## Sensors

Every material has `extras.nt.sensor`:

```json
{"class": "vegetation.sclerophyll",
 "lidar": {"reflectance905": 0.42, "reflectance1550": 0.22},
 "thermal": {"emissivityLwir": 0.97, "solarAbsorptivity": 0.5}}
```

The values are typical for the class (leaves are bright in the near infrared and darker at 1550 nm, where water
absorbs; bark follows its colour) and are meant as a starting point to override from measurements. Temperature
itself is not modelled here: it comes from the simulation.

Branches are modelled down to about 1.5 mm in radius at `LOD0`, and leaves are polygons at their real size, so a
lidar sees gaps where there are gaps. The leaf placement is statistical, not a measured tree: use a scan where the
position of each branch matters.

## Calibration against Tallo

The heights, trunk and crown proportions of the trees are checked against the Tallo allometry database (Jucker et al.
2022, <https://doi.org/10.5281/zenodo.6637599>, CC BY 4.0), with `calibrate_tallo.py --tallo Tallo.csv`. It takes the
species in its own region (the British Isles or Australia, then Europe, then the world) and the genus if the species
has too few records, and prints the 50th and 90th percentile of the height of mature trees and percentiles of
`trunk_radius_ratio` and `crown_radius`. Species whose numbers came from it say so in `species.h`: the height is the
50th to 90th percentile of mature trees, the trunk ratio the 75th percentile and the crown ratio the 90th, because
inventories are mostly of trees in woods and ntgentree grows trees that stand alone. The gums, wattle and she-oak are
not calibrated: Australia has too few records of those species, and a river red gum from the plantations of
other countries is not one from the Murray.

## Contact sheet

`contact_sheet.py --tool ntgentree --out sheet.png [--textures DIR]` grows every species and draws them on one sheet
(NumPy, Matplotlib and Pillow). With textures, a surface is drawn in the average colour of its image, with the image
beside it.

## Textures

`Textures` in `geometry.h` looks for `<species>_<slot>.png` (or `.jpg`) in the `--textures` directory, then for
`<slot>.png`. The slots are `bark` and `leaf` for trees, `blade` for grasses and `leaf` for weeds. The image is
embedded as it is; a PNG with an alpha channel makes the material an alpha cut-out. Leaf and blade images must have
the plant pointing up, its base at the bottom and the plant filling the frame. Bark repeats around a trunk once for
each half metre of its circumference, and once for each half metre along it.

`fetch_textures.py` makes them with fal.ai (Recraft V3, then `fal-ai/imageutils/rembg` with `crop_to_bbox` for the
cut-outs), reading `FAL_KEY` from the environment. Pillow, when installed, shrinks the images (`--max-size`) and
makes bark tile; NumPy is needed for that. `tests/gentree/test_fetch_textures.py` runs it against a local server.
Generated bark is not guaranteed to tile; look at the result.
