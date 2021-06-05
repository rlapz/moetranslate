#!/bin/sh

cc -g -Wall -Wextra moetranslate.c lib/cJSON.c  -lcurl -o moetranslate
