#!/bin/sh

cc -g moetranslate.c `pkg-config --cflags json-c` `pkg-config --libs json-c`  -lcurl -o moetranslate
