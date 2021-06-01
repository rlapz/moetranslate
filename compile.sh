#!/bin/sh

cc -Wall -Wextra moetranslate.c cJSON.c  -lcurl -o moetranslate
