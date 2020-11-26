#!/bin/bash
# Simple script to build the SWIG interface for HTM.  Relies on the HTM static library being precompiled.

OS=$(uname)

PYTHON_VERSION=$(python --version | awk '{print $2}'| awk -F. '{print $1"."$2}')

case "$OS" in

Darwin)  echo "Build for MacOS"
    ./clean.sh
    ( cd htm; ./build.sh )
    swig -c++ -python gkhtm.i
    export CLANG_FLAGS="-c -Wall -m64 -D_BOOL_EXISTS -UDIAGNOSE -Wno-deprecated -Wno-address-of-temporary -Wno-self-assign -D__macosx -arch x86_64"
    c++ -c HTMCircleRegion.cpp HTMCircleAllIDsCassandra.cpp -I./htm/include ${CLANG_FLAGS}
    c++ -c gkhtm_wrap.cxx -I./htm/include ${CLANG_FLAGS} -fno-strict-aliasing `python${PYTHON_VERSION}-config --includes`
    c++ -bundle -undefined dynamic_lookup -o _gkhtm.so HTMCircleRegion.o HTMCircleAllIDsCassandra.o gkhtm_wrap.o -lhtm -L./htm -arch x86_64 -L`python${PYTHON_VERSION}-config --prefix`/lib
    ;;
linux)  echo  "Build for Linux"
    ./clean.sh
    ( cd htm; ./build.sh )
    swig -c++ -python gkhtm.i
    export CFLAGS="-c -fPIC -g -Wall -fPIC -m64 -O2 -D_BOOL_EXISTS -D__unix -UDIAGNOSE -Wno-deprecated -fpermissive"
    c++ -c HTMCircleRegion.cpp HTMCircleAllIDsCassandra.cpp -I./htm/include ${CFLAGS}
    c++ -c gkhtm_wrap.cxx -I./htm/include ${CFLAGS} -fno-strict-aliasing `python${PYTHON_VERSION}-config --includes`
    c++ -shared  -o _gkhtm.so HTMCircleRegion.o HTMCircleAllIDsCassandra.o gkhtm_wrap.o -lhtm -L./htm -L`python${PYTHON_VERSION}-config --prefix`/lib
    ;;
*) echo "Unsupported OS"
   ;;
esac

