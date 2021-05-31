# moetranslate
A simple language translator written in C

##Currently support:
1. Google Translate API


##How to use:
`./moetranslate SOURCE_LANGUAGE TARGET_LANGUAGE [-b] "TEXT"`

`[-b] (optional) means "Brief Mode" (only show simple output)`

1. Brief mode:
	`./moetranslate en id -b "Hello world\!"`
2. Full mode:
	`./moetranslate en id "Hello wrld\!"`
	Will show translated WORD/SENTENCE with autocorrection (if needed) and
	more information.


## Third party libraries
1. [curl](https://github.com/curl/curl)
2. [jsmn](https://github.com/zserge/jsmn)

##TODO:
1. Add Yandex Translate API
