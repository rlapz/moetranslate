# moetranslate
Simple language translator written in C

## Currently Supported:
1. Google Translate API

## How to Compile:

```
chmod +x compile.sh
./compile.sh
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


## TODO:
1. Parse Full mode
2. Add Yandex Translate API
