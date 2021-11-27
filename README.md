# moetranslate
A simple language translator written in C


![](screenshots/ss1.png)

## Currently Supported:
1. Google Translate API

## Required Package(s):

```
libedit (https://thrysoee.dk/editline/)
```

## How to Install:

```
make install
```

## How to Uninstall:

```
make uninstall
```

## How to Use:

```
moetranslate [-b/-f/-r/-d/-h] [SOURCE] [TARGET] [TEXT]

-b = Brief output (only show simple output).
-f = Full/detail output.
-r = Raw output (json).
-i = Interactive input mode.
-d = Detect language.
-h = Show help message.
```


1. Brief output:
	`moetranslate -b auto:id "Hello world\!"`

	`auto` -> automatic detection

	`id`   -> Indonesian language code
2. Full/detail output:
	`moetranslate -f en:id "Hello wrld\!"`

	`en`   -> English language code

	Will show translated WORD/SENTENCE with more information.
3. Interactive input mode:
	```
	moetranslate -i
	moetranslate -i -b auto:en
	moetranslate -if auto:en
	```
4. Show help:
	`moetranslate -h`

## Language Code:
https://cloud.google.com/translate/docs/languages
