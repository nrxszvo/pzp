from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as build_ext_orig
from Cython.Build import cythonize
import os
import sys
import pathlib

class build_ext(build_ext_orig):
    def run(self):
        self.build_cmake(self.extensions[0].name)
        super().run()

    def build_cmake(self, name):
        cwd = pathlib.Path().absolute()
        # these dirs will be created in build_py, so if you don't have
        # any python sources to bundle, the dirs will be missing
        build_temp = pathlib.Path(self.build_temp)
        build_temp.mkdir(parents=True, exist_ok=True)

        # example of cmake args
        config = 'Debug' if self.debug else 'Release'
        cmake_args = [
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + str(build_temp.parent.absolute()),
            '-DCMAKE_BUILD_TYPE=' + config
        ]

        os.chdir(str(build_temp))
        self.spawn(['cmake', str(cwd / 'src' / name)] + cmake_args)
        if not self.dry_run:
            self.spawn(['cmake', '--build', '.'])
        # Troubleshooting: if fail on line above then delete all possible 
        # temporary CMake files including "CMakeCache.txt" in top level dir.
        os.chdir(str(cwd))  


# Get the directory containing this setup.py file
project_dir = os.path.join('src/pzp') 
build_dir = 'build'

# Library and include directories
if sys.platform == "darwin":
    system_lib_dir = "/opt/homebrew/lib"  # System libraries
    system_include_dir = "/opt/homebrew/include"  # System includes
else:
    system_lib_dir = "/usr/lib"  # System libraries
    system_include_dir = "/usr/include"  # System includes

extension = Extension(
    "pzp",
    sources=[os.path.join(project_dir, "pgnzstparser.pyx")],
    include_dirs=[
        project_dir,  # For parserPool.h
        system_include_dir,  # System headers
    ],
    libraries=[
        "pgnzstparser",
        "arrow",  # Arrow dependency
        "parquet",  # Parquet dependency
        "curl",  # Curl dependency
        "zstd",  # Zstd dependency
        "re2",  # RE2 dependency
    ],
    library_dirs=[
        system_lib_dir,  # For system libraries
        build_dir,
    ],
    runtime_library_dirs=[
        system_lib_dir,  # For system libraries
        build_dir,
    ],
    language="c++",
    extra_compile_args=["-std=c++20"],
    extra_link_args=[
        "-Wl,-rpath,@loader_path",  # Look for libraries in same dir as module
    ],
)

setup(
    ext_modules=cythonize(
        [extension],
        language_level=3,
    ),
    cmdclass={
        'build_ext': build_ext,
    },
    zip_safe=False  # Required for Cython extensions
)
