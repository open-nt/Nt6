#!/usr/bin/env python3
# MIT License - see LICENSE
"""Makes the textures of ntgentree with fal.ai.

ntgentree prints what each species needs with `ntgentree --textures-plan`: a prompt for the bark, the leaf or the
blade. This script runs that plan. Bark comes from Recraft V3; a leaf or a blade is generated on a plain white
background and then cut out, cropped to the plant and saved with an alpha channel, so it fills the leaf quad
exactly. Files are named <species>_<slot>.png, which is what `ntgentree --textures DIR` looks for.

The key is read from the FAL_KEY environment variable and is never printed or written. Each image is billed by
fal.ai; use --dry-run to see the prompts and the file names first, and --species to make a few at a time.

Pillow, if it is installed, converts WebP to PNG, shrinks the images to --max-size, and (with NumPy) makes the bark
tile without a seam. Without it the images are saved as they come.
"""

import argparse
import io
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request

TEXT_TO_IMAGE = "fal-ai/recraft/v3/text-to-image"
REMOVE_BACKGROUND = "fal-ai/imageutils/rembg"

PNG = b"\x89PNG\r\n\x1a\n"


class FalError(RuntimeError):
    pass


def request(url, data=None, headers=None, retries=4):
    """GETs or POSTs and returns the body, retrying when the server is busy."""
    for attempt in range(retries + 1):
        req = urllib.request.Request(url, data=data, headers=headers or {})
        try:
            with urllib.request.urlopen(req, timeout=300) as response:
                return response.read()
        except urllib.error.HTTPError as e:
            body = e.read()[:300].decode("utf-8", "replace")
            if e.code in (429, 500, 502, 503, 504) and attempt < retries:
                time.sleep(2 ** attempt)
                continue
            raise FalError("HTTP %d from %s: %s" % (e.code, url.split("?")[0], body))
        except urllib.error.URLError as e:
            if attempt < retries:
                time.sleep(2 ** attempt)
                continue
            raise FalError("cannot reach %s: %s" % (url.split("?")[0], e.reason))


def call(base, model, payload, key):
    body = request(
        "%s/%s" % (base.rstrip("/"), model),
        json.dumps(payload).encode("utf-8"),
        {"Authorization": "Key " + key, "Content-Type": "application/json"},
    )
    return json.loads(body)


def to_png(data, max_size, tile):
    """Returns PNG bytes. Needs Pillow for anything but a PNG that is left as it is."""
    try:
        from PIL import Image
    except ImportError:
        if data.startswith(PNG) or data.startswith(b"\xff\xd8\xff"):
            return data
        raise FalError("the image is neither PNG nor JPEG; install Pillow to convert it")
    img = Image.open(io.BytesIO(data))
    img.load()
    if img.mode not in ("RGB", "RGBA"):
        img = img.convert("RGBA" if "A" in img.getbands() or "transparency" in img.info else "RGB")
    if max(img.size) > max_size:
        scale = max_size / max(img.size)
        img = img.resize((max(1, round(img.width * scale)), max(1, round(img.height * scale))), Image.LANCZOS)
    if tile:
        img = make_tileable(img)
    out = io.BytesIO()
    img.save(out, "PNG", optimize=True)
    return out.getvalue()


def make_tileable(img):
    """Blends the image with itself shifted by half, so that its edges are the same as its inside."""
    try:
        import numpy as np
        from PIL import Image
    except ImportError:
        return img
    a = np.asarray(img, dtype=np.float32)
    shifted = np.roll(a, (a.shape[0] // 2, a.shape[1] // 2), axis=(0, 1))
    ty = 1.0 - np.abs(np.linspace(-1.0, 1.0, a.shape[0]))
    tx = 1.0 - np.abs(np.linspace(-1.0, 1.0, a.shape[1]))
    mask = np.outer(ty, tx)[:, :, None]
    out = a * mask + shifted * (1.0 - mask)
    return Image.fromarray(np.clip(out + 0.5, 0, 255).astype("uint8"), img.mode)


def fetch_image(base, key, item, max_size):
    payload = {"prompt": item["prompt"], "image_size": item["image_size"], "style": "realistic_image"}
    result = call(base, TEXT_TO_IMAGE, payload, key)
    url = result["images"][0]["url"]
    if item["alpha"]:
        result = call(base, REMOVE_BACKGROUND, {"image_url": url, "crop_to_bbox": True}, key)
        url = result["image"]["url"]
    return to_png(request(url), max_size, tile=item["slot"] == "bark")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--tool", default="ntgentree", help="the ntgentree executable (default: from PATH)")
    p.add_argument("--out", required=True, help="the directory for the textures")
    p.add_argument("--species", nargs="*", default=[], help="only these species")
    p.add_argument("--slot", choices=["bark", "leaf", "blade"], help="only this kind of texture")
    p.add_argument("--max-size", type=int, default=1024, help="longest side in pixels (needs Pillow)")
    p.add_argument("--force", action="store_true", help="make textures that already exist again")
    p.add_argument("--dry-run", action="store_true", help="print the prompts and file names and do nothing else")
    p.add_argument("--base-url", default=os.environ.get("NT_FAL_URL", "https://fal.run"), help=argparse.SUPPRESS)
    args = p.parse_args(argv)

    try:
        plan = json.loads(subprocess.run([args.tool, "--textures-plan"], check=True, capture_output=True).stdout)
    except (OSError, subprocess.CalledProcessError) as e:
        print("cannot run %s --textures-plan: %s" % (args.tool, e), file=sys.stderr)
        return 1
    known = {item["id"] for item in plan}
    unknown = [s for s in args.species if s not in known]
    if unknown:
        print("unknown species: %s (see ntgentree --list)" % ", ".join(unknown), file=sys.stderr)
        return 1
    plan = [i for i in plan if (not args.species or i["id"] in args.species) and (not args.slot or i["slot"] == args.slot)]

    key = os.environ.get("FAL_KEY", "")
    if not key and not args.dry_run:
        print("set FAL_KEY to your fal.ai key (or use --dry-run)", file=sys.stderr)
        return 1
    os.makedirs(args.out, exist_ok=True)

    made = failed = 0
    for item in plan:
        path = os.path.join(args.out, "%s_%s.png" % (item["id"], item["slot"]))
        if os.path.exists(path) and not args.force:
            print("have  " + path)
            continue
        if args.dry_run:
            print("%s\n  %s%s\n" % (path, item["prompt"], "  [cut out]" if item["alpha"] else ""))
            continue
        try:
            data = fetch_image(args.base_url, key, item, args.max_size)
        except (FalError, KeyError, IndexError, ValueError) as e:
            print("failed %s: %s" % (path, e), file=sys.stderr)
            failed += 1
            continue
        with open(path, "wb") as f:
            f.write(data)
        print("made  %s (%d bytes)" % (path, len(data)))
        made += 1
    if not args.dry_run:
        print("%d made, %d failed" % (made, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
