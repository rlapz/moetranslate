# moetranslate
Simple language translator written in C

## Currently Support:
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
	`./moetranslate auto id -b "Hello world\!"`

	`auto`	-> automatic detection
	`id`	-> Indonsian language code
2. Full mode:
	`./moetranslate en id "Hello wrld\!"`

	`en`	-> English US language code

	Will show translated WORD/SENTENCE with autocorrection (if needed) and
	more information.

## Language Code:
https://cloud.google.com/translate/docs/languages


## TODO:
1. Parse Full mode
2. Add Yandex Translate API
