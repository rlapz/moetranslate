/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

/* DO WHATEVER YOU WANT! */


/*** Interactive input mode configurations ***/
/*********************************************/
/* Output Mode: BRIEF, DETAIL, RAW, DETECT_LANG */
/*                                               source , target */
static const char *const     default_lang[]   = { "auto", "en" };
static const enum OutputMode out_default_mode = DETAIL;
#define PROMPT  "-> "  /* Prompt label */


/*** Global configurations ***/
/*****************************/
#define TEXT_MAX_LEN         4096 /* max input text length          */

#define DEFINITION_MAX_LINE  -1   /* definition max lines, 0 = disable, -1 = show all */
#define EXAMPLE_MAX_LINE     5    /* example max lines,    0 = disable, -1 = show all */
#define SYNONYM_MAX_LINE     -1   /* synonym max lines,    0 = disable, -1 = show all */


#define TIMEOUT              10   /* set request timeout (10s) */


#define USER_AGENT           "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_5) " \
			     "AppleWebKit/537.31 (KHTML, like Gecko) "         \
			     "Chrome/26.0.1410.65 Safari/537.31"               \


#define BASE_URL             "https://translate.googleapis.com/translate_a/single?"

#define URL_BRIEF            BASE_URL "client=gtx&ie=UTF-8&oe=UTF-8&dt=t&sl=%s&tl=%s&q=%s"

#define URL_DETAIL           BASE_URL "client=gtx&ie=UTF-8&oe=UTF-8&dt=bd&"                \
	                              "dt=ex&dt=ld&dt=md&dt=rw&dt=rm&dt=ss&"               \
                                      "dt=t&dt=at&dt=gt&dt=qca&sl=%s&tl=%s&hl=%s&q=%s"

#define URL_DETECT_LANG      BASE_URL "client=gtx&sl=auto&q=%s"


/* colors */
/* see: https://en.wikipedia.org/wiki/ANSI_escape_code */
#define BLUE_COLOR           "34"
#define GREEN_COLOR          "32"
#define WHITE_COLOR          "37"
#define YELLOW_COLOR         "33"

/* 17+1 109 */
static const struct Lang lang[] = {
{"auto", "Automatic"},

{"af" , "Afrikaans"   }, {"sq"   , "Albanian"           }, {"am"   , "Amharic"             },
{"ar" , "Arabic"      }, {"hy"   , "Armenian"           }, {"az"   , "Azerbaijani"         },
{"eu" , "Basque"      }, {"be"   , "Belarusian"         }, {"bn"   , "Bengali"             },
{"bs" , "Bosnian"     }, {"bg"   , "Bulgarian"          }, {"ca"   , "Catalan"             },
{"ceb", "Cebuano"     }, {"zh-CN", "Chinese Simplified" }, {"zh-TW", "Chinese Traditional" },
{"co" , "Corsican"    }, {"hr"   , "Croatian"           }, {"cs"   , "Czech"               },
{"da" , "Danish"      }, {"nl"   , "Dutch"              }, {"en"   , "English"             },
{"eo" , "Esperanto"   }, {"et"   , "Estonian"           }, {"fi"   , "Finnish"             },
{"fr" , "French"      }, {"fy"   , "Frisian"            }, {"gl"   , "Galician"            },
{"ka" , "Georgian"    }, {"de"   , "German"             }, {"el"   , "Greek"               },
{"gu" , "Gujarati"    }, {"ht"   , "Haitian Crole"      }, {"ha"   , "Hausan"              },
{"haw", "Hawaiian"    }, {"iw"   , "Hebrew"             }, {"hi"   , "Hindi"               },
{"hmn", "Hmong"       }, {"hu"   , "Hungarian"          }, {"is"   , "Icelandic"           },
{"ig" , "Igbo"        }, {"id"   , "Indonesian"         }, {"ga"   , "Irish"               },
{"it" , "Italian"     }, {"ja"   , "Japanese"           }, {"jv"   , "Javanese"            },
{"kn" , "Kannada"     }, {"kk"   , "Kazakh"             }, {"km"   , "Khmer"               },
{"rw" , "Kinyarwanda" }, {"ko"   , "Korean"             }, {"ku"   , "Kurdish"             },
{"ky" , "Kyrgyz"      }, {"lo"   , "Lao"                }, {"la"   , "Latin"               },
{"lv" , "Latvian"     }, {"lt"   , "Lithunian"          }, {"lb"   , "Luxembourgish"       },
{"mk" , "Macedonian"  }, {"mg"   , "Malagasy"           }, {"ms"   , "Malay"               },
{"ml" , "Malayam"     }, {"mt"   , "Maltese"            }, {"mi"   , "Maori"               },
{"mr" , "Marathi"     }, {"mn"   , "Mongolian"          }, {"my"   , "Myanmar"             },
{"ne" , "Nepali"      }, {"no"   , "Norwegian"          }, {"ny"   , "Nyanja"              },
{"or" , "Odia"        }, {"ps"   , "Pashto"             }, {"fa"   , "Persian"             },
{"pl" , "Polish"      }, {"pt"   , "Portuguese"         }, {"pa"   , "Punjabi"             },
{"ro" , "Romanian"    }, {"ru"   , "Russian"            }, {"sm"   , "Samoan"              },
{"gd" , "Scots Gaelic"}, {"sr"   , "Serbian"            }, {"st"   , "Sesotho"             },
{"sn" , "Shona"       }, {"sd"   , "Sindhi"             }, {"si"   , "Sinhala"             },
{"sk" , "Slovak"      }, {"sl"   , "Slovenian"          }, {"so"   , "Somali"              },
{"es" , "Spanish"     }, {"su"   , "Sundanese"          }, {"sw"   , "Swahili"             },
{"sv" , "Swedish"     }, {"tl"   , "Tagalog"            }, {"tg"   , "Tajik"               },
{"ta" , "Tamil"       }, {"tt"   , "Tatar"              }, {"te"   , "Telugu"              },
{"th" , "Thai"        }, {"tr"   , "Turkish"            }, {"tk"   , "Turkmen"             },
{"uk" , "Ukranian"    }, {"ur"   , "Urdu"               }, {"ug"   , "Uyghur"              },
{"uz" , "Uzbek"       }, {"vi"   , "Vietnamese"         }, {"cy"   , "Welsh"               },
{"xh" , "Xhosa"       }, {"yi"   , "Yiddish"            }, {"yo"   , "Yaruba"              },
{"zu" , "Zulu"        } };

