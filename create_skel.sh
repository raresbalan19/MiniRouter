#!/bin/bash

rm -rf skel.zip
zip -r skel.zip Makefile src/ checker/ arp_table.txt rtable0.txt rtable1.txt create_archive.sh
