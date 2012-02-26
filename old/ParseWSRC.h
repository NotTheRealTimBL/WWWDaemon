/*			Parse WAIS Source file
**			======================
*/

extern int WSRC_init 	NOPARAMS;
extern int WSRC_treat	PARAMS((char c));
extern int WSRC_gen_html NOPARAMS;
extern char WSRC_address[256];
extern char * enslash	PARAMS((CONST char * s));
extern char * deslash	PARAMS((CONST char * db));
extern char * hex;
extern char from_hex	PARAMS((char c));


#define HEX_ESCAPE '%'

#define WAIS_CACHE_ROOT	"/usr/local/lib/WAIS/"
