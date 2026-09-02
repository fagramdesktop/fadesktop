#!/bin/bash
set -e

cd Telegram
./configure.sh "$@"
cmake --build ../out --config "${CONFIG:-MinSizeRel}" ${KEEP_GOING:+-- -k 0}

([[ -d ../out/install ]] && rm -rf ../out/install; mkdir -p ../out/install) && DESTDIR=../out/install cmake --install ../out
