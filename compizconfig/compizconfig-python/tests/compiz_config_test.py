import os
import sys
import subprocess

arch = subprocess.Popen (["uname", "-p"], stdout=subprocess.PIPE).communicate ()[0][:-1]

os.environ["G_SLICE"] = "always-malloc"
os.environ["COMPIZ_METADATA_PATH"] = "compizconfig/compizconfig-python/"
os.environ["COMPIZ_CONFIG_PROFILE"] = ""
os.environ["XDG_CONFIG_HOME"] = "compizconfig/libcompizconfig/config"
os.environ["LIBCOMPIZCONFIG_BACKEND_PATH"] = "compizconfig/libcompizconfig/backend/"
os.environ["XDG_DATA_DIRS"] = "generated/"

sys.path.insert (0, "compizconfig/compizconfig-python")

import unittest
import compizconfig

class CompizConfigTest (unittest.TestCase):

    ccs = compizconfig

    def setUp (self):
        self.context = compizconfig.Context ()

