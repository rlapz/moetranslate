# moetranslate
A simple language translator written in C

## Currently Support:
1. Google Translate API

## How to Compile:

```
cc moestranslate.c -lcurl -o moetranslate
```

## How to Use:

```
./moetranslate SOURCE_LANGUAGE TARGET_LANGUAGE [-b] "TEXT"

[-b] (optional) means "Brief Mode" (only show simple output)
```


1. Brief mode:
	`./moetranslate en id -b "Hello world\!"`
2. Full mode:
	`./moetranslate en id "Hello wrld\!"`
	Will show translated WORD/SENTENCE with autocorrection (if needed) and
	more information.


## Third Party Libraries
1. [curl](https://github.com/curl/curl)
2. [jsmn](https://github.com/zserge/jsmn)

## TODO:
1. Parse Full mode
2. Add Yandex Translate API
