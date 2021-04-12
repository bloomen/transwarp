from conans import ConanFile
from conans.tools import download, unzip
import os


class TranswarpConan(ConanFile):
    name = "transwarp"
    version = "2.2.2"
    description = "Conan package for bloomen/transwarp."
    url = "https://github.com/bloomen/transwarp"
    license = "MIT"
    settings = "arch", "build_type", "compiler", "os"
    generators = "cmake"

    def source(self):
        zip_name = "%s.zip" % self.version
        download("%s/archive/%s" % (self.url, zip_name), zip_name, verify=False)
        unzip(zip_name)
        os.unlink(zip_name)

    def package(self):
        include_folder = "%s-%s/include" % (self.name, self.version)
        self.copy("transwarp.h", dst="include", src=include_folder)
