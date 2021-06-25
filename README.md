# moetranslate
Simple language translator written in C

## Currently Supported:
1. Google Translate API

## How to Install:

```
make && sudo make install
```

## How to Uninstall:

```
sudo make uninstall
```

## How to Use:

```
moetranslate SOURCE_LANGUAGE TARGET_LANGUAGE [-b] "TEXT"

[-b] (optional) means "Brief Mode" (only show simple output)
```


1. Brief mode:
	`moetranslate auto id -b "Hello world\!"`

	`auto`	-> automatic detection
	`id`	-> Indonsian language code
2. Full mode:
	`moetranslate en id "Hello wrld\!"`

	`en`	-> English language code

	Will show translated WORD/SENTENCE with	more information.

## Language Code:
https://cloud.google.com/translate/docs/languages
