/* MIT License
 *
 * Copyright (c) 2024 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */


/*
 * Langs: see Lang section below
 */
#define CONFIG_LANG_INDEX_SRC (0)
#define CONFIG_LANG_INDEX_TRG (21)


/*
 * Result type: RESULT_TYPE_DETAIL, RESULT_TYPE_SIMPLE, RESULT_TYPE_LANG
 */
#define CONFIG_RESULT_TYPE RESULT_TYPE_DETAIL


/*
 * Prompt label (interactive mode)
 */
#define CONFIG_INTERACTIVE_HISTORY_SIZE (128u)


/*
 * DEF: Definition
 * EXM: Example
 * SYN: Synonym
 *
 * -1 : Show all (no restriction)
 */
#define CONFIG_DEF_LINES_MAX (-1)
#define CONFIG_EXM_LINES_MAX (-1)
#define CONFIG_SYN_LINES_MAX (-1)

#define CONFIG_EXM_BUFFER_SIZE (1024u)


/*
 * Colors
 * See: https://en.wikipedia.org/wiki/ANSI_escape_code
 */
#define CONFIG_COLOR_ENABLED (1)

#define CONFIG_COLOR_BLUE   "34"
#define CONFIG_COLOR_GREEN  "32"
#define CONFIG_COLOR_WHITE  "39"
#define CONFIG_COLOR_YELLOW "33"


/*
 * Internal
 */
#define CONFIG_BUFFER_SIZE       (4096u)
#define CONFIG_BUFFER_MAX_SIZE   ((1024u * 1024u) * 8u)
#define CONFIG_PRINT_BUFFER_SIZE BUFSIZ

#define CONFIG_HTTP_HOST   "translate.googleapis.com"
#define CONFIG_HTTP_PORT   "80"
#define CONFIG_HTTP_METHOD "GET "

#define CONFIG_HTTP_PATH_BASE   "/translate_a/single?client=gtx&ie=UTF-8"
#define CONFIG_HTTP_PATH_SIMPLE "&oe=UTF-8&dt=t"
#define CONFIG_HTTP_PATH_DETAIL "&oe=UTF-8&dt=bd&dt=ex&dt=ld&dt=md&dt=rw&dt=rm&dt=ss"\
				"&dt=t&dt=at&dt=gt&dt=qca"
#define CONFIG_HTTP_PATH_LANG   "&sl=auto"

#define CONFIG_HTTP_QUERY_SL  "&sl="
#define CONFIG_HTTP_QUERY_TL  "&tl="
#define CONFIG_HTTP_QUERY_HL  "&hl="
#define CONFIG_HTTP_QUERY_TXT "&q="

#define CONFIG_HTTP_PROTOCOL " HTTP/1.1\r\n"
#define CONFIG_HTTP_HEADER   "Host: " CONFIG_HTTP_HOST "\r\n"\
                             "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_5) "\
			     "AppleWebKit/537.31 (KHTML, like Gecko) "\
			     "Chrome/26.0.1410.65 Safari/537.31\r\n"\
                             "Connection: Close\r\n\r\n"


/*
 * Lang
 */
#define CONFIG_LANG_KEY_SIZE (5)

#define CONFIG_LANG_PACK {\
	/* [index] = { key, value } */							 \
											 \
	[0]   = { "auto",  "Automatic"           },					 \
	          									 \
	[1]   = { "af",    "Afrikaans"           }, [2]   = { "sq",    "Albanian"          },\
	[3]   = { "am",    "Amharic"             }, [4]   = { "ar",    "Arabic"            },\
	[5]   = { "hy",    "Armenian"            }, [6]   = { "az",    "Azerbaijani"       },\
	[7]   = { "eu",    "Basque"              }, [8]   = { "be",    "Belarusian"        },\
	[9]   = { "bn",    "Bengali"             }, [10]  = { "bs",    "Bosnian"           },\
	[11]  = { "bg",    "Bulgarian"           }, [12]  = { "ca",    "Catalan"           },\
	[13]  = { "ceb",   "Cebuano"             }, [14]  = { "zh-CN", "Chinese Simplified"},\
	[15]  = { "zh-TW", "Chinese Traditional" }, [16]  = { "co",    "Corsican"          },\
	[17]  = { "hr",    "Croatian"            }, [18]  = { "cs",    "Czech"             },\
	[19]  = { "da",    "Danish"              }, [20]  = { "nl",    "Dutch"             },\
	[21]  = { "en",    "English"             }, [22]  = { "eo",    "Esperanto"         },\
	[23]  = { "et",    "Estonian"            }, [24]  = { "fi",    "Finnish"           },\
	[25]  = { "fr",    "French"              }, [26]  = { "fy",    "Frisian"           },\
	[27]  = { "gl",    "Galician"            }, [28]  = { "ka",    "Georgian"          },\
	[29]  = { "de",    "German"              }, [30]  = { "el",    "Greek"             },\
	[31]  = { "gu",    "Gujarati"            }, [32]  = { "ht",    "Haitian Crole"     },\
	[33]  = { "ha",    "Hausan"              }, [34]  = { "haw",   "Hawaiian"          },\
	[35]  = { "iw",    "Hebrew"              }, [36]  = { "hi",    "Hindi"             },\
	[37]  = { "hmn",   "Hmong"               }, [38]  = { "hu",    "Hungarian"         },\
	[39]  = { "is",    "Icelandic"           }, [40]  = { "ig",    "Igbo"              },\
	[41]  = { "id",    "Indonesian"          }, [42]  = { "ga",    "Irish"             },\
	[43]  = { "it",    "Italian"             }, [44]  = { "ja",    "Japanese"          },\
	[45]  = { "jw",    "Javanese"            }, [46]  = { "kn",    "Kannada"           },\
	[47]  = { "kk",    "Kazakh"              }, [48]  = { "km",    "Khmer"             },\
	[49]  = { "rw",    "Kinyarwanda"         }, [50]  = { "ko",    "Korean"            },\
	[51]  = { "ku",    "Kurdish"             }, [52]  = { "ky",    "Kyrgyz"            },\
	[53]  = { "lo",    "Lao"                 }, [54]  = { "la",    "Latin"             },\
	[55]  = { "lv",    "Latvian"             }, [56]  = { "lt",    "Lithunian"         },\
	[57]  = { "lb",    "Luxembourgish"       }, [58]  = { "mk",    "Macedonian"        },\
	[59]  = { "mg",    "Malagasy"            }, [60]  = { "ms",    "Malay"             },\
	[61]  = { "ml",    "Malayam"             }, [62]  = { "mt",    "Maltese"           },\
	[63]  = { "mi",    "Maori"               }, [64]  = { "mr",    "Marathi"           },\
	[65]  = { "mn",    "Mongolian"           }, [66]  = { "my",    "Myanmar"           },\
	[67]  = { "ne",    "Nepali"              }, [68]  = { "no",    "Norwegian"         },\
	[69]  = { "ny",    "Nyanja"              }, [70]  = { "or",    "Odia"              },\
	[71]  = { "ps",    "Pashto"              }, [72]  = { "fa",    "Persian"           },\
	[73]  = { "pl",    "Polish"              }, [74]  = { "pt",    "Portuguese"        },\
	[75]  = { "pa",    "Punjabi"             }, [76]  = { "ro",    "Romanian"          },\
	[77]  = { "ru",    "Russian"             }, [78]  = { "sm",    "Samoan"            },\
	[79]  = { "gd",    "Scots Gaelic"        }, [80]  = { "sr",    "Serbian"           },\
	[81]  = { "st",    "Sesotho"             }, [82]  = { "sn",    "Shona"             },\
	[83]  = { "sd",    "Sindhi"              }, [84]  = { "si",    "Sinhala"           },\
	[85]  = { "sk",    "Slovak"              }, [86]  = { "sl",    "Slovenian"         },\
	[87]  = { "so",    "Somali"              }, [88]  = { "es",    "Spanish"           },\
	[89]  = { "su",    "Sundanese"           }, [90]  = { "sw",    "Swahili"           },\
	[91]  = { "sv",    "Swedish"             }, [92]  = { "tl",    "Tagalog"           },\
	[93]  = { "tg",    "Tajik"               }, [94]  = { "ta",    "Tamil"             },\
	[95]  = { "tt",    "Tatar"               }, [96]  = { "te",    "Telugu"            },\
	[97]  = { "th",    "Thai"                }, [98]  = { "tr",    "Turkish"           },\
	[99]  = { "tk",    "Turkmen"             }, [100] = { "uk",    "Ukranian"          },\
	[101] = { "ur",    "Urdu"                }, [102] = { "ug",    "Uyghur"            },\
	[103] = { "uz",    "Uzbek"               }, [104] = { "vi",    "Vietnamese"        },\
	[105] = { "cy",    "Welsh"               }, [106] = { "xh",    "Xhosa"             },\
	[107] = { "yi",    "Yiddish"             }, [108] = { "yo",    "Yaruba"            },\
	[109] = { "zu",    "Zulu"                },\
}
