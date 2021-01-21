import platform
from setuptools import setup, Extension, find_packages
import os
import glob

moduleDirectory = os.path.dirname(os.path.realpath(__file__))

#sources = glob.glob('src/*.cpp') + glob.glob('src/*.i')

def readme():
    with open(moduleDirectory + '/README.md') as f:
        return f.read()

if platform.system() == 'Darwin':
    # MacOS
    extra_compile_args = ['-c', '-g', '-D_BOOL_EXISTS', '-D__macosx', '-UDIAGNOSE', '-Wno-deprecated', '-Wno-self-assign', '-Wno-address-of-temporary', '-Wno-format', '-Wno-dangling-else', '-Wno-unused-private-field', '-arch', 'x86_64', '-stdlib=libc++']
    extra_link_args = ['-arch', 'x86_64']
    from distutils.sysconfig import get_config_var
    from distutils.version import LooseVersion
    if 'MACOSX_DEPLOYMENT_TARGET' not in os.environ:
        current_system = LooseVersion(platform.mac_ver()[0])
        python_target = LooseVersion(
            get_config_var('MACOSX_DEPLOYMENT_TARGET'))
        if python_target < '10.9' and current_system >= '10.9':
            os.environ['MACOSX_DEPLOYMENT_TARGET'] = '10.9'
else:
    # Assumes Linux
    extra_compile_args = ['-c', '-fPIC', '-g', '-Wall', '-D_BOOL_EXISTS', '-D__unix', '-UDIAGNOSE', '-Wno-deprecated', '-fpermissive']
    extra_link_args = []

static_include_dirs = ['gkhtm/htm/include']
plib_include_dirs = static_include_dirs.copy()
plib_include_dirs.append('gkhtm')
htm_sources = glob.glob('gkhtm/htm/src/*.cpp')
# Missing .c file
htm_sources.append('gkhtm/htm/cc_aux.c')
libraries = ['htm']
library_dirs = ['build']


# 2020-06-17 KWS Attempt to build the static library.  This took many hours of figuring out, but
#                after doing so, I realised that we have all the .o files necessary to build into
#                the python shared library, so in fact, this code is not necessary.  I've left it
#                here for posterity.
from setuptools import Command

sources = htm_sources.copy()

from distutils.ccompiler import new_compiler
from sysconfig import get_paths 
import os

project_name = "htm"
build_dir = os.path.join(os.path.dirname(__file__), 'build')

class BuildStaticLib(Command): 
    description = 'build static lib' 
    user_options = [] # do not remove, needs to be stubbed out! 
    python_info = get_paths() 

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        # Create compiler with default options
        print("Compiling Static Library...")
        c = new_compiler()

        # Add include directories etc.
        for d in static_include_dirs:
            c.add_include_dir(d)

        # Don't need python includes for this library, but leave in
        # for future reference.
        #c.add_include_dir(self.python_info['include'])

        # Compile into .o files
        objects = c.compile(sources, extra_postargs = extra_compile_args)

        # Create static or shared library
        c.create_static_lib(objects, project_name, output_dir=build_dir)


# Note to self - do NOT specify the swig output _wrap.cpp file.  This will cause the compiler
# to compile it twice and then attempt to link it twice.  This file is implicit from the call
# to swig.
# ALSO - swig version 3.0+ is required.  This does NOT work with version 2 for some reason.

htm_sources += ['gkhtm/gkhtm.i', 'gkhtm/HTMCircleRegion.cpp', 'gkhtm/HTMCircleAllIDsCassandra.cpp']

#htm_module = Extension('gkhtm._gkhtm', swig_opts=['-modern', '-c++'], extra_compile_args=extra_compile_args, extra_link_args=extra_link_args, sources=htm_sources, include_dirs=plib_include_dirs, libraries = libraries, library_dirs = library_dirs)
htm_module = Extension('gkhtm._gkhtm', swig_opts=['-modern', '-c++'], extra_compile_args=extra_compile_args, extra_link_args=extra_link_args, sources=htm_sources, include_dirs=plib_include_dirs)

ext_modules = []
ext_modules.append(htm_module)
packages = ['gkhtm']


setup(
    name="gkhtm",
    description='HTM library interface for Python',
    long_description=readme(),
    cmdclass = {'buildstatic': BuildStaticLib},
    ext_modules=ext_modules,
    long_description_content_type="text/markdown",
    version="0.0.7",
    author='genghisken',
    author_email='ken.w.smith@gmail.com',
    license='MIT',
    url='https://github.com/genghisken/gkhtm',
    packages=find_packages(),
    classifiers=[
          'Development Status :: 4 - Beta',
          'License :: OSI Approved :: MIT License',
          'Programming Language :: Python :: 3.6',
          'Topic :: Utilities',
    ],
    python_requires='>=3.6',
    include_package_data=True,
    zip_safe=False
)
