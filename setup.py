#!/usr/bin/env python3

import os, platform, shutil, sys, subprocess, glob
from distutils.command.bdist import bdist as _bdist
from distutils.command.sdist import sdist as _sdist
from distutils.command.build import build as _build
from distutils.command.clean import clean as _clean
from setuptools.command.egg_info import egg_info as _egg_info
import distutils.cmd
import distutils.log
from setuptools import setup, Extension, find_packages
from wheel.bdist_wheel import bdist_wheel as _bdist_wheel

debug = 'DQCSIM_DEBUG' in os.environ

target_dir = os.getcwd() + "/target"
py_target_dir = target_dir + "/python"
include_dir = target_dir + "/include"
output_dir = target_dir + ("/debug" if debug else "/release")
build_dir = py_target_dir + "/build"
dist_dir = py_target_dir + "/dist"

def build_tests():
    with open('python/dqcsim_cqasm/test.py', 'w') as f:
        f.write("""
# GENERATED FILE!

import unittest
from dqcsim.plugin import *
from dqcsim.host import *
import tempfile
import os

class Tests(unittest.TestCase):
    _indentation_error = None
    """)

        for filename in glob.glob('tests/valid/**/*.qasm', recursive=True):
            name = '_'.join(filename.split(os.sep)[2:]).split('.')[0].replace('-', '_')
            with open(filename, 'r') as fc:
                cqasm = fc.read()
            f.write("""
    def test_{name}(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            fname = tmpdir + os.sep + 'test_{name}.cq'
            with open(fname, 'w') as f:
                f.write({cqasm})
            with Simulator(fname, 'null') as sim:
                sim.run()
            """.format(name=name, cqasm=repr(cqasm)))

        for filename in glob.glob('tests/invalid/**/*.qasm', recursive=True):
            name = '_'.join(filename.split(os.sep)[2:]).split('.')[0].replace('-', '_')
            with open(filename, 'r') as fc:
                cqasm = fc.read()
            f.write("""
    def test_{name}(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            fname = tmpdir + os.sep + 'test_{name}.cq'
            with open(fname, 'w') as f:
                f.write({cqasm})
            with self.assertRaises(RuntimeError):
                with Simulator(fname, 'null') as sim:
                    sim.run()
            """.format(name=name, cqasm=repr(cqasm)))

def read(fname):
    with open(os.path.join(os.path.dirname(__file__), fname)) as f:
        return f.read()

class clean(_clean):
    def run(self):
        _clean.run(self)
        shutil.rmtree(target_dir)

class build(_build):
    def initialize_options(self):
        _build.initialize_options(self)
        self.build_base = build_dir

    def run(self):
        from plumbum import local, FG
        build_tests()
        local['mkdir']("-p", output_dir)
        if platform.system() == "Darwin":
            # brew install flex bison
            local.env["PATH"] = "/usr/local/opt/flex/bin/:/usr/local/opt/bison/bin:" + local.env["PATH"]
            # brew install llvm
            local.env["CXX"] = "/usr/local/opt/llvm/bin/clang++"

        with local.cwd(output_dir):
            if debug:
                local['cmake']['../..']['-DCMAKE_BUILD_TYPE=Debug'] & FG
            else:
                local['cmake']['../..'] & FG
            local['make']['-j']['4'] & FG

        _build.run(self)

class bdist_wheel(_bdist_wheel):
    def finalize_options(self):
        _bdist_wheel.finalize_options(self)
        self.root_is_pure = False

    def get_tag(self):
        python, abi, plat = _bdist_wheel.get_tag(self)
        python, abi = 'py3', 'none'
        return python, abi, plat

class bdist(_bdist):
    def finalize_options(self):
        _bdist.finalize_options(self)
        self.dist_dir = dist_dir

class sdist(_sdist):
    def finalize_options(self):
        _sdist.finalize_options(self)
        self.dist_dir = dist_dir

class egg_info(_egg_info):
    def initialize_options(self):
        _egg_info.initialize_options(self)
        self.egg_base = py_target_dir

setup(
    name = 'dqcsim-cqasm',
    version = '0.0.2',

    author = 'Quantum Computer Architectures, Quantum & Computer Engineering, QuTech, Delft University of Technology',
    author_email = '',
    description = 'A DQCsim frontend for cQASM files',
    license = "Apache-2.0",
    url = "https://github.com/QE-Lab/dqcsim-cqasm",
    project_urls = {
        "Source Code": "https://github.com/QE-Lab/dqcsim-cqasm",
    },

    long_description = read('README.md'),
    long_description_content_type = 'text/markdown',

    classifiers = [
        "License :: OSI Approved :: Apache Software License",

        "Operating System :: POSIX :: Linux",

        "Programming Language :: C",
        "Programming Language :: C++",

        "Topic :: Scientific/Engineering"
    ],

    data_files = [
        ('bin', [
            output_dir + '/dqcsfecq',
        ]),
    ],

    packages = find_packages('python'),
    package_dir = {
        '': 'python',
    },

    cmdclass = {
        'bdist': bdist,
        'bdist_wheel': bdist_wheel,
        'build': build,
        'clean': clean,
        'egg_info': egg_info,
        'sdist': sdist,
    },

    setup_requires = [
        'plumbum',
    ],

    install_requires = [
        'dqcsim',
    ],

    tests_require = [
        'nose',
    ],
    test_suite = 'nose.collector',

    zip_safe = False,
)
