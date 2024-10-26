from setuptools import setup
from Cython.Build import cythonize

setup(
    ext_modules=cythonize("sgp4_module.pyx"),
)