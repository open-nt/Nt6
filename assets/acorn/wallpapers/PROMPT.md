# Generating release artwork

The Acorn artwork is drawn by `make_wallpaper.py` (the crown, hexagonal scales on the golden spiral). For a release
whose subject does not reduce to geometry as well, generate the artwork instead and place it with
`make_wallpaper.py --image <file>`. On [fal.ai](https://fal.ai) use **Recraft V3** with SVG output (the
vector-illustration style; the same prompt works for a raster model such as FLUX or Ideogram if an SVG is not needed), a
**1:1** aspect ratio and a plain background. The prompt below, for a whole acorn, is the worked example; the
`nt-icon-style` skill has the template and the rules it follows.

## Prompt

```
A single large acorn, isolated and centered, lying on the diagonal like a fallen acorn: the pointed tip at the lower
left ending in a tiny dark nib, the big domed cup at the upper right, and a short stub stem with a flat cut face
pointing up and to the right. The cup is covered in neat overlapping diamond-shaped scales in offset rows that
shrink towards the crown, with a thick rounded rolled rim where it meets the nut. The nut is smooth, plump and glossy
with faint lengthwise grain, a soft broad highlight along its upper left and a gentle shadow under the rim.

Colour palette, strictly: the nut in teal #13977F shading to deep teal #0C6655; the cup and rim in mint neon #3FD9B4
shading to #2BB894 in the crevices between the scales; the stem in deep teal #0C6655; a solid dark pine green
background #0B2E27. Nothing brown, orange or yellow.

Style: premium 3D-looking vector illustration, smooth gradients, soft studio lighting from the upper left with a
subtle mint rim light from the right, crisp clean edges, no outlines, no text, no leaves, no background objects,
no floor, generous margin around the acorn.
```

## Negative prompt (where the model takes one)

```
brown, orange, yellow, beige, realistic bark, photograph, leaves, oak leaf, text, letters, watermark, outline strokes,
cartoon, flat clipart, multiple acorns, floor, shadow on the ground, busy background, noise, grain
```

## Settings

| Setting | Value |
|---|---|
| Model | Recraft V3, SVG / vector illustration |
| Size | 1024 × 1024 (1:1) |
| Colours | If the model accepts a colour list, give `#13977F`, `#0C6655`, `#3FD9B4`, `#2BB894`, `#0B2E27` |
| Background | Solid `#0B2E27`, or transparent if offered; the script keys out a flat pine background |

## Placing it

    assets/<release>/wallpapers/make_wallpaper.py --image assets/<release>/wallpapers/<subject>.svg --name <Nut> --version <x.y.z>

The file is embedded unchanged into the full and the clean wallpapers, in the crown's place. Keep it in the output
directory so that `rsvg-convert` is allowed to read it.
