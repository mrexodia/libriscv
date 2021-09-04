#!/bin/bash
set -e
GCC_TRIPLE="riscv64-linux-gnu"
export CC=$GCC_TRIPLE-gcc-10
export CXX=$GCC_TRIPLE-g++-10
NIMCPU="--cpu=riscv64"
NIMFILE="$PWD/${1:-hello.nim}"

mkdir -p $GCC_TRIPLE
pushd $GCC_TRIPLE

NIMCACHE=$PWD/nimcache
mkdir -p $NIMCACHE

# find nim and replace /bin/nim with /lib
NIM_LIBS=`whereis nim`
NIM_LIBS="${NIM_LIBS##*: }"
NIM_LIBS="${NIM_LIBS/bin*/lib}"

nim c --nimcache:$NIMCACHE $NIMCPU --colors:on --os:linux --gc:arc -d:useMalloc --threads:off -d:release -c ${NIMFILE}
jq '.compile[] [0]' $NIMCACHE/*.json > buildfiles.txt

cmake .. -G Ninja -DGCC_TRIPLE=$GCC_TRIPLE -DNIM_LIBS=$NIM_LIBS
ninja
popd

# print the filename
echo $GCC_TRIPLE/`cat $GCC_TRIPLE/program.txt`