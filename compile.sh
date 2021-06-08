#!/bin/sh

cc -Wall -Wextra moetranslate.c lib/cJSON.c  -lcurl -o moetranslate -O3
