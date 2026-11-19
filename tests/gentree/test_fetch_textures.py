# MIT License - see LICENSE
"""Tests tools/ntgentree/fetch_textures.py against a local server that stands in for fal.ai.

usage: test_fetch_textures.py <ntgentree> <fetch_textures.py>
"""

import http.server
import json
import os
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
import zlib

NTGENTREE = sys.argv.pop(1)
SCRIPT = sys.argv.pop(1)


def make_png(w, h, rgba):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(rgba) * w for _ in range(h))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


class Fal(http.server.BaseHTTPRequestHandler):
    calls = []
    fail_next = 0

    def log_message(self, *args):
        pass

    def reply(self, code, body, kind="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        payload = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        Fal.calls.append((self.path, self.headers.get("Authorization"), payload))
        if Fal.fail_next:
            Fal.fail_next -= 1
            return self.reply(400, b'{"detail":"nope"}')
        host = "http://127.0.0.1:%d" % self.server.server_address[1]
        if self.path.endswith("/recraft/v3/text-to-image"):
            return self.reply(200, json.dumps({"images": [{"url": host + "/files/raw.png"}]}).encode())
        if self.path.endswith("/imageutils/rembg"):
            return self.reply(200, json.dumps({"image": {"url": host + "/files/cut.png"}}).encode())
        self.reply(404, b"{}")

    def do_GET(self):
        if self.path == "/files/raw.png":
            return self.reply(200, make_png(4, 4, (90, 70, 50, 255)), "image/png")
        if self.path == "/files/cut.png":
            return self.reply(200, make_png(2, 4, (40, 120, 40, 200)), "image/png")
        self.reply(404, b"")


class FetchTextures(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Fal)
        cls.url = "http://127.0.0.1:%d" % cls.server.server_address[1]
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def setUp(self):
        Fal.calls.clear()
        Fal.fail_next = 0
        self.dir = tempfile.TemporaryDirectory()
        self.env = dict(os.environ, FAL_KEY="test-key")

    def tearDown(self):
        self.dir.cleanup()

    def run_script(self, *args, key=True):
        env = dict(self.env)
        if not key:
            env.pop("FAL_KEY", None)
        return subprocess.run(
            [sys.executable, SCRIPT, "--tool", NTGENTREE, "--out", self.dir.name, "--base-url", self.url, *args],
            env=env, capture_output=True, text=True,
        )

    def test_dry_run_needs_no_key_and_calls_nothing(self):
        r = self.run_script("--dry-run", "--species", "quercus_robur", key=False)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("quercus_robur_bark.png", r.stdout)
        self.assertIn("quercus_robur_leaf.png", r.stdout)
        self.assertIn("English oak", r.stdout)
        self.assertEqual(Fal.calls, [])
        self.assertEqual(os.listdir(self.dir.name), [])

    def test_missing_key(self):
        r = self.run_script("--species", "quercus_robur", key=False)
        self.assertEqual(r.returncode, 1)
        self.assertIn("FAL_KEY", r.stderr)

    def test_unknown_species(self):
        r = self.run_script("--dry-run", "--species", "no_such_plant")
        self.assertEqual(r.returncode, 1)
        self.assertIn("unknown species", r.stderr)

    def test_bark_is_one_call_and_a_leaf_is_cut_out(self):
        r = self.run_script("--species", "quercus_robur")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(sorted(os.listdir(self.dir.name)), ["quercus_robur_bark.png", "quercus_robur_leaf.png"])
        paths = [c[0] for c in Fal.calls]
        self.assertEqual(paths.count("/fal-ai/recraft/v3/text-to-image"), 2)
        self.assertEqual(paths.count("/fal-ai/imageutils/rembg"), 1)
        self.assertTrue(all(c[1] == "Key test-key" for c in Fal.calls))
        cut = [c[2] for c in Fal.calls if c[0].endswith("rembg")][0]
        self.assertTrue(cut["crop_to_bbox"])
        prompts = [c[2]["prompt"] for c in Fal.calls if c[0].endswith("text-to-image")]
        self.assertTrue(any("bark" in p and "Seamless" in p for p in prompts))
        self.assertTrue(any("white background" in p for p in prompts))
        # The key is not in anything the script prints.
        self.assertNotIn("test-key", r.stdout + r.stderr)
        for name in os.listdir(self.dir.name):
            with open(os.path.join(self.dir.name, name), "rb") as f:
                self.assertTrue(f.read().startswith(b"\x89PNG"))

    def test_existing_files_are_kept_unless_forced(self):
        self.assertEqual(self.run_script("--species", "themeda_triandra").returncode, 0)
        first = len(Fal.calls)
        self.assertGreater(first, 0)
        r = self.run_script("--species", "themeda_triandra")
        self.assertEqual(len(Fal.calls), first)
        self.assertIn("have", r.stdout)
        self.assertEqual(self.run_script("--species", "themeda_triandra", "--force").returncode, 0)
        self.assertGreater(len(Fal.calls), first)

    def test_slot_filter(self):
        self.assertEqual(self.run_script("--species", "quercus_robur", "fagus_sylvatica", "--slot", "bark").returncode, 0)
        self.assertEqual(sorted(os.listdir(self.dir.name)), ["fagus_sylvatica_bark.png", "quercus_robur_bark.png"])

    def test_a_failure_is_reported_and_the_rest_continue(self):
        Fal.fail_next = 1
        r = self.run_script("--species", "quercus_robur", "--slot", "bark")
        self.assertEqual(r.returncode, 1)
        self.assertIn("failed", r.stderr)
        self.assertIn("HTTP 400", r.stderr)
        self.assertEqual(os.listdir(self.dir.name), [])

    def test_the_result_is_what_ntgentree_embeds(self):
        self.assertEqual(self.run_script("--species", "quercus_robur").returncode, 0)
        glb = os.path.join(self.dir.name, "oak.glb")
        r = subprocess.run(
            [NTGENTREE, "quercus_robur", "--max-leaves", "200", "--textures", self.dir.name, "-o", glb],
            capture_output=True, text=True,
        )
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(glb, "rb") as f:
            data = f.read()
        length = struct.unpack_from("<I", data, 12)[0]
        doc = json.loads(data[20:20 + length])
        self.assertEqual(len(doc["images"]), 2)
        self.assertTrue(all(i["mimeType"] == "image/png" for i in doc["images"]))


if __name__ == "__main__":
    unittest.main()
