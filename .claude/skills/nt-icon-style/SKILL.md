---
name: nt-icon-style
description: The visual theme for every Nt icon, illustration, wallpaper, social tile, mascot and release artwork, and how to generate it with fal.ai (Recraft V3 SVG) or draw it. Use this skill whenever the user asks for an icon, logo-adjacent artwork, illustration, wallpaper, hero image, badge, app or module icon, favicon, release name artwork (releases are named after nuts, 6.0.0 is Acorn), or an image-generation prompt for anything Nt — even if they only say "make a picture of X" or "we need art for the release".
---

# Nt icon and illustration style

Nt's mark is a teal hexagonal nut. Everything drawn for Nt grows from that: the same five colours, the hexagon as a
recurring shape, and a premium, softly lit, three-dimensional vector look — never flat clipart, never a photograph.
The brand guide (`doc/brand.h`) owns the logo rules; this skill owns everything drawn next to the logo.

## The palette, and nothing else

| Name | Hex | Role |
|---|---|---|
| Pine | `#0B2E27` | Backgrounds, text on neon, the lines between scales and facets |
| Deep Pine | `#06201B` | Vignettes, the darkest shadows |
| Nut Teal | `#13977F` | The main body colour of a subject; links |
| Deep Teal | `#0C6655` | The shaded side of a teal body, stems, crevices |
| Nut Neon | `#3FD9B4` | Highlights, rims, the brightest surfaces, headline text on pine |
| Neon Deep | `#2BB894` | Neon in shadow |
| White | `#FFFFFF` | Specular highlights at low opacity, body text |
| Muted | `#A6CDC3` | Captions, footers |

Nothing brown, orange, yellow, red or blue. A subject that is naturally another colour (an acorn, wood, a flame) is
re-coloured into teal and neon; that is the point of the theme. Qt's green (`#41CD52`) and its cut-corner shapes are
off limits, so that nothing drawn for Nt is mistaken for Qt.

Typography is Titillium Web: Semibold for names and versions, Bold for a release name as a headline, Regular for
captions. Text never goes inside generated artwork; it is set afterwards, as a layer.

## The look

- **Form:** a single subject, isolated, centred or on a diagonal, with a generous margin (about a quarter of its
  size) on every side. Clean closed silhouettes, no outline strokes.
- **Volume:** smooth gradients that read as real volume, like a well-made 3D render traced as vectors. Studio light
  from the upper left, a soft broad highlight, a subtle mint rim light from the right, and a gentle contact shadow
  where parts meet. No hard drop shadows, no floor.
- **Hexagons:** where a subject has texture or facets (scales, tiles, cells, grids), use hexagons or diamonds in
  offset rows; it ties the piece to the mark. Backgrounds may carry a faint hexagon grid at very low contrast.
- **Finish:** crisp, polished, confident. If it would look at home on a sticker sheet, it is too flat; if it has
  photographic grain, it has gone too far.

## Drawn or generated

A subject that reduces to geometry is drawn procedurally, in Python that writes SVG: the 6.0.0 Acorn artwork is the
*Acorn crown*, the cup seen from above as hexagonal scales on the golden-angle spiral (`crown()` in
`assets/acorn/wallpapers/make_wallpaper.py`). Repetition with a rule (spirals, lattices, rings, facets) looks
designed; a hand-built figurative shape (a whole acorn, a leaf, a face) looks like clipart however carefully it is
shaded. For figurative subjects, generate instead: write the prompt from this template, hand it to the user for
fal.ai (or run it through an image-generation tool if one is available in the session), then place the result.
Either way, lettering goes into an SVG as outlines (`assets/acorn/wallpapers/ttfpaths.py`), never as `<text>` that
depends on the viewer having the font.

**Model and settings:** Recraft V3 on fal.ai with SVG output (vector-illustration style); 1:1 for icons and subjects,
16:9 only for finished scenes; the five main hex colours in the colour list when the model accepts one; a solid Pine
background (or transparent). The same prompt works for a raster model when SVG is not needed.

**Prompt template** — fill every slot; keep the colour and style paragraphs verbatim:

```
A single {SUBJECT}, isolated and centered, {POSE AND COMPOSITION}. {TWO OR THREE SENTENCES OF CONCRETE DETAIL: the
parts, their textures as hexagonal or diamond patterns where there is texture, how the light falls on them}.

Colour palette, strictly: {THE MAIN BODY} in teal #13977F shading to deep teal #0C6655; {THE BRIGHT PARTS} in mint
neon #3FD9B4 shading to #2BB894 in the crevices; a solid dark pine green background #0B2E27. Nothing brown, orange,
yellow, red or blue.

Style: premium 3D-looking vector illustration, smooth gradients, soft studio lighting from the upper left with a
subtle mint rim light from the right, crisp clean edges, no outlines, no text, no background objects, no floor,
generous margin around the subject.
```

**Negative prompt** (where the model takes one):

```
brown, orange, yellow, red, blue, beige, photograph, realistic texture, text, letters, watermark, outline strokes,
cartoon, flat clipart, multiple subjects, floor, ground shadow, busy background, noise, grain
```

Fill the slots with nouns and geometry, not adjectives: "the cup covered in overlapping diamond scales in offset rows
that shrink towards the crown" steers a model; "beautiful detailed cup" does not. Read `references/examples.md` for
the worked Acorn prompt and for subject ideas per release.

## Checking the result

Run `scripts/check_palette.py <image>` on the generated PNG (or on the SVG, which it rasterises). It reports how much
of the picture falls outside the palette; more than a few percent means the model drifted to brown or grey and the
prompt needs the colour paragraph restated or the colour list set. Then look at it: one subject, margin all round,
light from the upper left, no text baked in.

## Placing it

- **Release wallpapers:** each release has `assets/<release>/wallpapers/` (6.0.0: `assets/acorn/wallpapers/`).
  Copy the Acorn directory, keep the release's artwork file there and run
  `make_wallpaper.py --image <file> --version <x.y.z> --name <Nut> --out assets/<release>/wallpapers`. It composes
  the background, the artwork, the mark with the version beside it and the release name, in landscape and portrait,
  plus a `-clean` variant with the artwork alone. Releases are named after nuts (6.0.0 Acorn); the list is in the
  examples. A procedural artwork replaces `crown()` instead of using `--image`.
- **Icons:** a release's icon set lives in `assets/<release>/icon/` (see `make_icon.py`, which thins the crown as
  the size drops); other icons go under `assets/icons/<name>.svg`. Square, with the subject filling about 70% of
  the canvas. Export PNGs with `rsvg-convert -w <px>` at the sizes the target needs (16, 32, 48, 64, 128, 256, 512
  for desktop icon sets; 1024 for stores). Icons are MIT licensed like the rest of the tree; say so in the commit.
- **Social and docs images:** the tile scripts in `assets/social/` already carry the palette constants; place
  generated subjects into them rather than redrawing.

## When drawing by hand is still right

Flat, tiny or purely geometric things — favicons, badges, status glyphs, the hexagon grid itself — are drawn as plain
SVG: Pine or Neon fills, no gradients, 2 px minimum features at 16 px. Keep those to the palette and the hexagon
motif and they will sit next to the generated pieces without a seam.
