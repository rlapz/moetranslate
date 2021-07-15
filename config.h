/* MIT License
 *
 * Copyright (c) 2021 Arthur Lapz (rLapz)
 *
 * See LICENSE file for license details
 */

#define BUFFER_SIZE 4096
static const int example_max_line	= 5;	/* example lines */
static const long timeout		= 10L;	/* set request timeout (10s) */
static const char user_agent[]		= "libcurl-agent/1.0";
static const Url url_google = {
	"https://translate.googleapis.com/translate_a/single?", /* base url */
	{
		[BRIEF]	= "client=gtx&ie=UTF-8&oe=UTF-8&dt=t", /* url parameter (brief) */
		[FULL]	= "client=gtx&ie=UTF-8&oe=UTF-8&dt=bd&dt=ex&dt=ld&dt=md&dt=rw&" /* url parameter (full) */
			  "dt=rm&dt=ss&dt=t&dt=at&dt=gt&dt=qca"
	}
};

/* 17+1 109 */
static const Language language[] = {
	{"auto", "Automatic"},

	{"af", "Afrikaans"},	{"sq", "Albanian"},	{"am", "Amharic"},
	{"ar", "Arabic"},	{"hy", "Armenian"},	{"az", "Azerbaijani"},
	{"eu", "Basque"},	{"be", "Belarusian"},	{"bn", "Bengali"},
	{"bs", "Bosnian"},	{"bg", "Bulgarian"},	{"ca", "Catalan"},
	{"ceb", "Cebuano"},	{"zh-CN", "Chinese Simplified"}, {"zh-TW", "Chinese Traditional"},
	{"co", "Corsican"},	{"hr", "Croatian"},	{"cs", "Czech"},
	{"da", "Danish"},	{"nl", "Dutch"},	{"en", "English"},
	{"eo", "Esperanto"},	{"et", "Estonian"},	{"fi", "Finnish"},
	{"fr", "French"},	{"fy", "Frisian"},	{"gl", "Galician"},
	{"ka", "Georgian"},	{"de", "German"},	{"el", "Greek"},
	{"gu", "Gujarati"},	{"ht", "Haitian Crole"}, {"ha", "Hausan"},
	{"haw", "Hawaiian"},	{"he", "Hebrew"},	{"hi", "Hindi"},
	{"hmn", "Hmong"},	{"hu", "Hungarian"},	{"is", "Icelandic"},
	{"ig", "Igbo"},		{"id", "Indonesian"},	{"ga", "Irish"},
	{"it", "Italian"},	{"ja", "Japanese"},	{"jv", "Javanese"},
	{"kn", "Kannada"},	{"kk", "Kazakh"},	{"km", "Khmer"},
	{"rw", "Kinyarwanda"},	{"ko", "Korean"},	{"ku", "Kurdish"},
	{"ky", "Kyrgyz"},	{"lo", "Lao"},		{"la", "Latin"},
	{"la", "Latvian"},	{"lt", "Lithunian"},	{"lb", "Luxembourgish"},
	{"mk", "Macedonian"},	{"mg", "Malagasy"},	{"ms", "Malay"},
	{"ml", "Malayam"},	{"mt", "Maltese"},	{"mi", "Maori"},
	{"mr", "Marathi"},	{"mn", "Mongolian"},	{"my", "Myanmar"},
	{"ne", "Nepali"},	{"no", "Norwegian"},	{"ny", "Nyanja"},
	{"or", "Odia"},		{"ps", "Pashto"},	{"fa", "Persian"},
	{"pl", "Polish"},	{"pt", "Portuguese"},	{"pa", "Punjabi"},
	{"ro", "Romanian"},	{"ru", "Russian"},	{"sm", "Samoan"},
	{"gd", "Scots Gaelic"},	{"sr", "Serbian"},	{"st", "Sesotho"},
	{"sn", "Shona"},	{"sd", "Sindhi"},	{"si", "Sinhala"},
	{"sk", "Slovak"},	{"sl", "Slovenian"},	{"so", "Somali"},
	{"es", "Spanish"},	{"su", "Sundanese"},	{"sw", "Swahili"},
	{"sv", "Swedish"},	{"tl", "Tagalog"},	{"tg", "Tajik"},
	{"ta", "Tamil"},	{"tt", "Tatar"},	{"te", "Telugu"},
	{"th", "Thai"},		{"tr", "Turkish"},	{"tk", "Turkmen"},
	{"uk", "Ukranian"},	{"ur", "Urdu"},		{"ug", "Uyghur"},
	{"uz", "Uzbek"},	{"vi", "Vietnamese"},	{"cy", "Welsh"},
	{"xh", "Xhosa"},	{"yi", "Yiddish"},	{"yo", "Yaruba"},
	{"zu", "Zulu"},
};

