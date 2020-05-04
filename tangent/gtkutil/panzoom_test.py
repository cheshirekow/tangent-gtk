#!/usr/bin/env python
"""
Test builder JSON/serialization by executing the demo application on a
UI file with all the supported data models.
"""

import argparse
import copy
import io
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

logger = logging.getLogger(__name__)

GLOBAL = {
    "bindir": None,
    "viscat": False
}

TESTDATA = {
    "builder_models": {
        "offset_x_adjustment": 0,
        "offset_y_adjustment": 0,
        "scale_adjustment": 1,
        "scale_rate_adjustment": 1.1,
    }
}

# adapted from https://stackoverflow.com/a/30228308/141023


def hcat_images(infile_paths, outfile_path):
  from PIL import Image
  images = list(map(Image.open, infile_paths))
  widths, heights = zip(*(i.size for i in images))

  total_width = sum(widths)
  max_height = max(heights)
  new_img = Image.new("RGBA", (total_width, max_height))

  x_offset = 0
  for img in images:
    new_img.paste(img, (x_offset, 0))
    x_offset += img.size[0]

  new_img.save(outfile_path)


class TestPanzoom(unittest.TestCase):
  def __init__(self, *args, **kwargs):
    super(TestPanzoom, self).__init__(*args, **kwargs)
    self.tempdir = None

  def setUp(self):
    self.tempdir = tempfile.mkdtemp(prefix="panzoom_test_")
    logger.debug("Writing tempfiles to %s", self.tempdir)

  def tearDown(self):
    if GLOBAL["viscat"]:
      infile_paths = [
          os.path.join(self.tempdir, "{:02}.png".format(idx))
          for idx in range(4)]
      outfile_path = os.path.join(self.tempdir, "cat.png")
      hcat_images(infile_paths, outfile_path)
      subprocess.check_call(["eog", outfile_path])
    shutil.rmtree(self.tempdir)

  def popen(self, output_filename, glade_filename, expect_hash):
    output_filepath = os.path.join(self.tempdir, output_filename)
    env = os.environ.copy()
    # NOTE(josh): if we don't provide an empty/invalid dbus address, our
    # application will hang for several seconds trying to connect to dbus
    # before starting
    env["DBUS_SESSION_BUS_ADDRESS"] = ""
    proc = subprocess.Popen(
        ["./gtk-panzoom-demo",
         "--input", "-", "--ui", glade_filename, "test",
         "--outfile", output_filepath, expect_hash],
        stdin=subprocess.PIPE,
        cwd=GLOBAL["bindir"],
        env=env)
    proc.stdin = io.TextIOWrapper(proc.stdin, encoding="utf-8")
    return proc

  def execute_demo(self, glade_filename):
    indata = copy.deepcopy(TESTDATA)
    models = indata["builder_models"]

    with self.subTest(state="nominal"):
      expect_hash = ("af926b00b5fd9affc286b3b4b1b1a9cda9acc29c"
                     "adc5a6b9b4a4b0b4b5b1a7bbaea8b6aeaeb4afb7")
      proc = self.popen("00.png", glade_filename, expect_hash)
      json.dump(indata, proc.stdin)
      proc.stdin.write("\n")
      proc.stdin.close()
      self.assertEqual(0, proc.wait())

    with self.subTest(state="offset"):
      expect_hash = ("860042ff6c5a8eb469769a8a77966ca1826f9b85"
                     "7c8f7c91847d8d888581888c80848a868781878c")
      proc = self.popen("01.png", glade_filename, expect_hash)
      models["offset_x_adjustment"] = 0.3
      models["offset_y_adjustment"] = 0.3
      json.dump(indata, proc.stdin)
      proc.stdin.write("\n")
      proc.stdin.close()
      self.assertEqual(0, proc.wait())

    with self.subTest(state="scaled up"):
      expect_hash = ("87de700007bb79ffde2c890c78d597c86b557b85"
                     "88929473a9906d6d749a91b57f5e8d6d939b858a")
      proc = self.popen("02.png", glade_filename, expect_hash)
      models["scale_adjustment"] = 0.5
      json.dump(indata, proc.stdin)
      proc.stdin.write("\n")
      proc.stdin.close()
      self.assertEqual(0, proc.wait())

    with self.subTest(state="scaled down"):
      expect_hash = ("780450ff0093ab4679917a46b17140c35360b04f"
                     "7c8f5d867670866a817b6b8672757e7579757c76")
      proc = self.popen("03.png", glade_filename, expect_hash)
      models["scale_adjustment"] = 2.0
      json.dump(indata, proc.stdin)
      proc.stdin.write("\n")
      proc.stdin.close()
      self.assertEqual(0, proc.wait())

  def test_panzoom_in_glade(self):
    self.execute_demo("panzoom.ui")

  def test_panzoom_in_code(self):
    self.execute_demo("panzoom-base.ui")


def main():
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "bindir",
      help="path to the cmake binary directory where the demo program and"
           " gladefiles are located")
  parser.add_argument(
      "--viscat", action="store_true",
      help="display the cat of the four images when done")
  parser.add_argument("remainder", nargs=argparse.REMAINDER)
  parser.add_argument(
      "-l", "--log-level", default="info",
      choices=["error", "warning", "info", "debug"])
  args = parser.parse_args()
  logging.getLogger().setLevel(getattr(logging, args.log_level.upper()))
  GLOBAL["bindir"] = args.bindir
  GLOBAL["viscat"] = args.viscat
  unittest.main(argv=([sys.argv[0]] + args.remainder))


if __name__ == "__main__":
  main()
