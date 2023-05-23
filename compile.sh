#!/bin/bash

gcc `pkg-config --cflags gtk4` *.c `pkg-config --libs gtk4` -lm -Wno-deprecated-declarations
