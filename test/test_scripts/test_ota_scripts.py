#!/usr/bin/env python3
"""
Unit tests for OTA scripts and build helpers.
Re-exports the comprehensive test suite from test/test_ota_scripts.py.
"""

import os
import sys

# Ensure the parent test directory is in the path
PARENT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if PARENT_DIR not in sys.path:
    sys.path.insert(0, PARENT_DIR)

from test_ota_scripts import *  # noqa: F401, F403

if __name__ == "__main__":
    import unittest
    unittest.main()
