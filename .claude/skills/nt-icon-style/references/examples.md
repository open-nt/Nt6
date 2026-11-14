# Examples and subjects

## The Acorn artwork (Nt 6.0.0)

The shipped 6.0.0 artwork is drawn, not generated: the *Acorn crown*, 190 hexagonal scales placed on the golden-angle
spiral (137.508° per step, radius growing with the square root of the index, as in a sunflower head or an acorn cup
seen from above), each scale turning with its angle, shading from Nut Neon at the centre to Deep Teal at the rim with
a small upper-left bevel, over a soft neon glow. Twelve procedural options were compared on a contact sheet before
choosing it; the ones that lost were rings, random fields and anything with a literal acorn silhouette.

## The acorn prompt

This is the filled template for a whole acorn, the worked example for a generated subject;
`assets/acorn/wallpapers/PROMPT.md` keeps a copy next to the wallpapers.

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

What made it work: the pose is described as positions on the canvas (tip lower left, cup upper right), the texture is
named as a geometric pattern (diamond scales in offset rows), the colours are assigned to parts, and the forbidden
colours are spelled out because an acorn is brown in every training image.

## Release names

Releases are named after nuts, in the order they ship. 6.0.0 is Acorn. Suggested next names, each with a subject that
suits the style:

| Name | Subject notes |
|---|---|
| Hazelnut | Round nut in a frilled cup; the frill as overlapping hexagonal leaves |
| Walnut | The shell's ridges as a raised hexagonal network; show it whole, not cracked |
| Pecan | Elongated, with a faceted shell; a diagonal pose like the acorn |
| Chestnut | Glossy nut with a pale base; the spiky husk half open behind it, spikes as fine neon lines |
| Almond | Simple teardrop; let the gradient do the work, keep the pitted shell texture as hexagons |
| Pistachio | Half-open shell, the kernel in neon showing through the gap |
| Macadamia | A sphere in a cracked sphere; the crack as a clean hexagonal opening |
| Pine nut | A nod to the Pine background; a cone with hexagonal scales behind a small kernel |
| Cashew | The kidney curve on the diagonal; smooth, almost no texture |
| Beechnut | Three-sided nut in a four-lobed husk; crisp facets |

## Module and tool icons

A module icon is the module's idea as one object, in the same light: Nt Network as a hexagonal mesh of nodes, Nt SQL
as stacked hexagonal plates, Nt Multimedia as a speaker cone made of concentric hexagons, `ntmoc` as a hexagon being
stamped out of a sheet. Fill the template with that object; keep the mark itself out of the picture (it stands next
to the icon, never inside it).
