#!/bin/bash

cd "$(dirname "$0")" || exit 1
cd ..

cd src
make
if [ $? != 0 ]; then
    echo "Make failed, bailing out..." >&2
    exit 1
fi
cd ..
mkdir -p build
mv src/router build/

sudo fuser -k 6653/tcp
sudo python3 checker/topo.py tests
