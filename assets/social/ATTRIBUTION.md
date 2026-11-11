# Social media tiles

`nt-social-*.png` are promotional tiles for Nt, made by `make_tiles.py` (the drawing helpers are in `tilelib.py`):

| File | Size | For |
|---|---|---|
| `nt-social-1-hero.png` | 1080 x 1080 | Instagram, LinkedIn, Mastodon, X |
| `nt-social-2-code.png` | 1080 x 1080 | the HTTP server example |
| `nt-social-3-modules.png` | 1080 x 1080 | the nine modules |
| `nt-social-4-widgets.png` | 1080 x 1080 | the widgets, dark and light |
| `nt-social-5-contribute.png` | 1080 x 1080 | contributors (uses the Nt Development logo) |
| `nt-social-6-card-1200x630.png` | 1200 x 630 | link previews (Open Graph, X card, LinkedIn) |
| `nt-social-7-banner-1500x500.png` | 1500 x 500 | profile banners (X, LinkedIn) |
| `nt-social-8-github-1280x640.png` | 1280 x 640 | the GitHub repository's social preview (Settings, Social preview) |

They follow the [Nt Brand Guide](../../doc/brand.h): Nut Teal, Nut Neon and Pine, Titillium Web, and the Nt mark with its
clear space. Every tile says that Nt is not affiliated with The Qt Company, or is a card too small for it, as the guide asks.

## LinkedIn series: Nt Explained

`linkedin/` holds eight posts that teach Nt one lesson at a time, all 1080 x 1350 (4:5), made by `make_linkedin.py`
(`TITILLIUM_DIR=<fonts> python3 make_linkedin.py linkedin`). Each image has its post text in a `.txt` file with the same name (`nt-linkedin-1-what-is-nt.png` and
`nt-linkedin-1-what-is-nt.txt`), ready to paste into LinkedIn; `linkedin/captions.md` has all eight together.
`linkedin/contact-sheet.png` shows the whole series on one page.

| Post | Lesson |
|---|---|
| 1 | What is Nt? |
| 2 | Clean-room and MIT, explained |
| 3 | Signals and slots, no moc (ntmoc) |
| 4 | Nine modules, built one by one |
| 5 | Three lines of CMake |
| 6 | A web service in six lines |
| 7 | Widgets, the Qt way (the first tab of `hello_widgets`) |
| 8 | Trust it, improve it (the Nt Development logo, as for contributor material) |
| 9 | Qt APIs, now on Zephyr (`make_zephyr_tiles.py`) |
| 10 | From QObject to firmware: the QEMU sample (`make_zephyr_tiles.py`) |

## Third-party material

- `zephyr-logo-dark.svg` is the Zephyr logo (`doc/_static/images/logo-readme-dark.svg` of the Zephyr project), used in tiles 9 and 10
  and in the five "Nt on Zephyr" posts in `linkedin/` (`make_linkedin_zephyr.py`, with a `.txt` caption beside each image) to
  say that Nt runs on that RTOS. Zephyr is licensed under Apache-2.0; the Zephyr name and logo are trademarks of The Linux
  Foundation, and their use here does not imply endorsement.

- `github-fontawesome.svg` is the **GitHub** icon from [Font Awesome Free](https://fontawesome.com) 6.7.2,
  Copyright 2024 Fonticons, Inc., licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). It is drawn in the footer of the tiles, unchanged except for
  its color. The GitHub name and logo are trademarks of GitHub, Inc.
- Titillium Web is licensed under the SIL Open Font License 1.1 (`../brand/OFL-TitilliumWeb.txt`). The fonts are not in the
  repository; the tiles have the letters drawn into the pictures.
