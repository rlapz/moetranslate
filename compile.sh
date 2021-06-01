#!/bin/sh

cc -g -Wall -Wextra moetranslate.c cJSON.c  -lcurl -o moetranslate
