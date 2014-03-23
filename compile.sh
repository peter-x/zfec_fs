#!/bin/sh
gcc -Wall -g zfec_fs.c zfec/fec.c `pkg-config fuse --cflags --libs` -o zfec_fs
