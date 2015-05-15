#!/bin/bash

set -e

this_dir=$(readlink -f `dirname ${BASH_SOURCE[0]}`)
cd $this_dir

project=`basename $this_dir`

includes=`echo | \
    clang++ -std=c++14 -v -E -x c++ - 2>&1 | \
    awk '/^#include/ {flag=1; next}
         /^End/      {flag=0}
         flag        {print}' | \
    xargs readlink -f | \
    awk '{printf "\"-isystem%s\",\n", $1}'
`
includes=`printf %q "$includes"`
includes="${includes#$\'}"
includes="${includes%\'}"

[[ -d BUILD ]] || mkdir BUILD

mkdir -p /tmp/$project-build
if which bindfs >/dev/null 2>&1; then
   bindfs -n /tmp/$project-build BUILD
else
   sudo mount -o bind /tmp/$project-build BUILD
fi

sed -f - .ycm_extra_conf.py.in > BUILD/.ycm_extra_conf.py <<END
s|@CMAKE_CURRENT_BINARY_DIR@|$this_dir/BUILD|
s|@SYSTEM_HEADERS@|$includes|
END

(
cd BUILD
CC=clang CXX=clang++ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
)
