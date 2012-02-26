/*			Parse WAIS Source file	O B S O L E T E   F I L E
**			======================
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "HTUtils.h"
#define BIG 10000		/* Arbitrary limit to value length */
#define CACHE_PERIOD (7*86400)	/* Time to keep .src file in seconds */
static char param[BIG];

typedef struct _par {
    char * name;
    char * value;
} source_parameter;

/*	Here are the parameters which can be specified in a  source file
*/
static source_parameter par[] = {
	"version", 		0,
	"ip-address", 		0,
#define PAR_IP_NAME 2
	"ip-name", 		0,
#define PAR_TCP_PORT 3
	"tcp-port", 		0,
#define PAR_DATABASE_NAME 4
	"database-name", 	0,
#define PAR_COST 5
	"cost", 		0,
#define PAR_COST_UNIT 6
	"cost-unit", 		0,
#define PAR_FREE 7
	"free",			0,
#define PAR_MAINTAINER 8
	"maintainer", 		0,
#define PAR_DESCRIPTION 9
	"description", 		0,
	0,			0,	/* Terminate list */
} ;

/*	Positions to after string in file.
**	---------------------------------
**
** On exit,
**	returns	0 if end of file,
**		1 if success
**	stdin	next char read will be first after the string.
*/
int find(const char * str)
{
    const char * p = str;
    for(;;) {
        char c = getc(stdin);
	if (c==EOF) return 0;
	if (c==*p) {
	    p++;
	    if (!*p) return 1;	/* Good */
	} else {
	    p = str;
	}
    }
}

/*	Read a paratemeter.
**	------------------
**
** On exit,
**	returns	0 if end of file,
**		param if success
**	stdin	next char read will be first after the parmeter.
**	param	global set to value read.
*/
char * get_param(void)
{
    char * p = param;
    BOOL quoted;
    for(;;) {
        char c = getc(stdin);
	if (c==EOF) return 0;
	if (!WHITE(c)) {
	    *p = c;
	    quoted = (c == '"');
	    if (!quoted) p++;
	    break;
	}
    }
    for(;;) {
        char c = getc(stdin);
	if (c==EOF) return 0;
	if (quoted ? (c=='"') : WHITE(c)) {
	    *p = 0;
	    return param;
	} else {
	    *p++ = c;
	}
    }
}


int main(int argc, char *argv[])
{
    int p;

    if (!find("(")) exit(-1);
    if (!find(":source")) exit(-2);	/* Check it's a source file */
    if (!get_param()) exit(-3);		/* Nothing follows? */
    for(;;) {
	if (param[0] != ':') {
	    fprintf(stderr, "Found %s when expecting ':' in wais source\n",
	    	param);
	    exit(-4);
	}
	for(p = 0; par[p].name; p++) {	/* Identify the parameter */
	    if (0==strcmp(par[p].name, param+1)) {
	        break;
	    }
	}
	
	if (!par[p].name) {		/* Unknown field */
	    if (!get_param()) break;	/* Skip it */
	    if (param[0] == ')') break;	/* end of list */
	    continue;
	}
	
	if (!get_param()) break;
	if (param[0] == ')') break;	/* end of list */
	if (param[0] == ':') continue;	/* No value, just flag */
	
	StrAllocCopy(par[p].value, param);
	if (!get_param()) break;
	if (param[0] == ')') break;	/* end of list */
    }
    
/*	Output equivalent HTML
*/

    {
        char address[256];
        char filename[256];
	FILE * html;
	struct stat buffer;		/* File status buffer */
	int status;
	
/*	Check whether a copy of this file already exists
*/
	sprintf(filename, "%s:%s:%s.html",
			par[PAR_IP_NAME].value,
			par[PAR_TCP_PORT].value,
			par[PAR_DATABASE_NAME].value);
	status = stat(filename, &buffer);
	if (status==0) {
	    extern long time(int *tloc);
	    if (buffer.st_mtime > time(0) - CACHE_PERIOD)
		exit(0);	/* Ok, recent file already exists */
	}

/*	If file does not exist, write it.
*/
	html = fopen(filename, "w");
	sprintf(address, "/%s:%s/%s",
			par[PAR_IP_NAME].value,
			par[PAR_TCP_PORT].value,
			par[PAR_DATABASE_NAME].value);
	
        fprintf(html, "<TITLE>%s index</TITLE>\n",
			par[PAR_DATABASE_NAME].value);
        fprintf(html, "<H1>%s/H1>\n",
			par[PAR_DATABASE_NAME].value);
	fprintf(html, "<XMP>%s\n", par[PAR_DESCRIPTION].value);
	fprintf(html, "</XMP>\n");
	fprintf(html, "[This database is maintained by %s on host %s.\n",
		par[PAR_MAINTAINER].value, par[PAR_IP_NAME].value);
	fprintf(html, "Its address is wais:/%s.]<p>\n", address);
	fprintf(html, "See <a href=%s>index</a></p>\n", address);
	
    } /* Output data */
    
    return 0;
    
} /* main */
