#!/usr/bin/env python
"""
Test builder JSON/serialization by executing the demo application on a
UI file with all the supported data models.
"""

import argparse
import io
import json
import logging
import os
import subprocess
import sys
import unittest

logger = logging.getLogger(__name__)


GLOBAL = {
    "builddir": None,
}

TESTDATA = {
    "builder_models": {
        "adjustment": 1.2340,
        "entrybuffer": "Hello World",
        "textbuffer":
            "Well, isn't this interesting\n""A multiline piece of test!\n",
        "togglebutton": True,
        "checkbutton": True,
        "radio-0": False,
        "radio-1": True,
        "radio-2": False,
        "colorbutton": "rgb(100,150,200)"
    }
}


class TestSerialization(unittest.TestCase):
  def test_serialize_glade(self):
    env = os.environ.copy()
    # NOTE(josh): if we don't provide an empty/invalid dbus address, our
    # application will hang for several seconds trying to connect to dbus
    # before starting
    env["DBUS_SESSION_BUS_ADDRESS"] = ""
    proc = subprocess.Popen(
        ["./gtk-serialize-demo", "serialize.ui",
         "--input", "-", "--output", "-", "--test"],
        cwd=GLOBAL["builddir"],
        stdout=subprocess.PIPE,
        stdin=subprocess.PIPE,
        env=env)
    try:
      code = proc.wait(timeout=0)
      self.assertIsNone(code)
    except subprocess.TimeoutExpired:
      pass
    stdin = io.TextIOWrapper(proc.stdin, encoding="utf-8")
    stdout = io.TextIOWrapper(proc.stdout, encoding="utf-8")

    json.dump(TESTDATA, stdin)
    stdin.write("\n")
    stdin.close()
    result = json.load(stdout)
    stdout.close()
    code = proc.wait()
    self.assertEqual(0, code)

    for key, value in TESTDATA.get("builder_models").items():
      self.assertEqual(result.get("builder_models", {}).get(key, None), value)
      logger.debug("%s: OK", key)


def main():
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      "builddir",
      help="path to the directory containing the gtk-serialize_demo binary and"
           " the serialize.ui glade file")
  parser.add_argument("remainder", nargs=argparse.REMAINDER)
  parser.add_argument(
      "-l", "--log-level", default="info",
      choices=["error", "warning", "info", "debug"])
  args = parser.parse_args()
  logging.getLogger().setLevel(getattr(logging, args.log_level.upper()))
  GLOBAL["builddir"] = args.builddir
  unittest.main(argv=([sys.argv[0]] + args.remainder))


if __name__ == "__main__":
  main()
