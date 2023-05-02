import builtins
import os
import pathlib
import setuptools
import setuptools.extension
import setuptools.command.build_ext
import sys

dirname = pathlib.Path(__file__).resolve().parent


class build_ext(setuptools.command.build_ext.build_ext):
    def finalize_options(self):
        setuptools.command.build_ext.build_ext.finalize_options(self)
        builtins.__NUMPY_SETUP__ = False  # type: ignore
        import numpy

        self.include_dirs.append(numpy.get_include())


extra_compile_args = []
extra_link_args = []
include_dirs = []
library_dirs = []
libraries = []
if sys.platform == "linux":
    extra_compile_args += ["-std=c++17", "-O3"]
    extra_link_args += ["-std=c++17", "-O3"]
    libraries += ["usb-1.0"]
elif sys.platform == "darwin":
    os.environ[
        "LDFLAGS"
    ] = "-framework IOKit -framework CoreFoundation -framework Security"
    extra_compile_args += ["-std=c++17", "-stdlib=libc++", "-O3"]
    extra_link_args += ["-std=c++17", "-stdlib=libc++", "-O3"]
    include_dirs += ["/usr/local/include", "/opt/homebrew/include"]
    library_dirs += ["/usr/local/lib", "/opt/homebrew/lib"]
    libraries += ["usb-1.0", "objc", "System"]
elif sys.platform == "win32":
    extra_compile_args += ["/std:c++17", "/O2"]
    library_dirs += [
        str(
            dirname
            / "third_party"
            / "bin"
            / f"win32_{'x64' if sys.maxsize > 2**32 else 'x86'}"
        )
    ]
    libraries += ["libusb-1.0"]

setuptools.setup(
    name="evk4_recorder_3d",
    version="1.0.0",
    description="Read events from a Prophesee Gen 4 dev kit 1.3 (Denebola)",
    long_description="Read events from a Prophesee Gen 4 dev kit 1.3 (Denebola)",
    long_description_content_type="text/markdown",
    setup_requires=["numpy"],
    install_requires=["numpy"],
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Operating System :: OS Independent",
    ],
    py_modules=["evk4_recorder_3d"],
    ext_modules=[
        setuptools.extension.Extension(
            "evk4_recorder_3d_extension",
            language="c++",
            sources=["evk4_recorder_3d_extension.cpp"],
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
            include_dirs=include_dirs,
            library_dirs=library_dirs,
            libraries=libraries,
        ),
    ],
    cmdclass={"build_ext": build_ext},
)
