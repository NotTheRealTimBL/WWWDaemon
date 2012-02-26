/*			Parse WAIS Source file
**			======================
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "HTUtils.h"

#include "ParseWSRC.h"

#define BIG 10000		/* Arbitrary limit to value length */
#define PARAM_MAX BIG
#define CACHE_PERIOD (7*86400)	/* Time to keep .src file in seconds */

extern char * deslash(CONST char * databasename);

typedef struct _par {
    char * name;
    char * value;
} source_parameter;

/*	Here are the parameters which can be specified in a  source file
*/
PRIVATE source_parameter par[] = {
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
	"source",		0,
	0,			0,	/* Terminate list */
} ;

enum tokenstate { beginning, before_tag, colon, junk_param, before_value,
		value, quoted_value, done };

PUBLIC char WSRC_address[256];

PRIVATE char * www_database;
PRIVATE state;
PRIVATE char param[BIG+1];
PRIVATE param_number;
PRIVATE param_count;

/*	Build the address
*/
void make_address()
{
    www_database = deslash(par[PAR_DATABASE_NAME].value);
    sprintf(WSRC_address, "/%s:%s/%s",
		par[PAR_IP_NAME].value,
		par[PAR_TCP_PORT].value,
		www_database);

}
PUBLIC char * hex = "0123456789ABCDEF";

/*	Decdoe one hex character
*/

PUBLIC char from_hex(char c)
{
    return 		  (c>='0')&&(c<='9') ? c-'0'
			: (c>='A')&&(c<='F') ? c-'A'+10
			: (c>='a')&&(c<='f') ? c-'a'+10
			:		       0;
}

/*	Turn database name into a path element
**	--------------------------------------
**
**	Creates new string which must be freed.
*/
PUBLIC char * deslash(CONST char * db)
{
#define ACCEPTABLE(c)	((c!='/')&&(c!='#')&&(c!='%')&&(c>' ')&&(c<='z'))
    CONST char * p;
    char * q;
    char * result;
    int unacceptable = 0;
    for(p=db; *p; p++)
        if (!ACCEPTABLE(*p))
		unacceptable++;
    result = (char *) malloc(p-db + unacceptable+ unacceptable + 1);
    for(q=result, p=db; *p; p++)
	if (!ACCEPTABLE(*p)) {
	    *q++ = HEX_ESCAPE;	/* Means hex commming */
	    *q++ = hex[(*p) >> 4];
	    *q++ = hex[(*p) & 15];
	}
	else *q++ = *p;
    *q++ = 0;			/* Terminate */
    return result;
}

/*	Reverse Operation
*/
/*		Return value must be freed
*/
PUBLIC char * enslash(CONST char * s)
{
    CONST char *p;
    char *z;
    char * result = (char *)malloc(strlen(s)+1);
    for(z=result, p=s; *p ;) {
	if (*p == HEX_ESCAPE) {
	    char c;
	    unsigned int b;
	    p++;
	    c = *p++;
	    b =   from_hex(c);
	    c = *p++;
	    if (!c) break;	/* Odd number of chars! */
	    *z++ = (b<<4) + from_hex(c);
	} else {
	    *z++ = *p++;	/* Non-hex */
	}
    }
    *z = 0;			/* Terminate */
    return result;
}

/*		State machine
**		-------------
**
** On entry,
**	state		is a valid state (see WSRC_init)
**	c		is the next character
** On exit,
** 	returns	1	Done with file
**		0	Continue. state is updated if necessary.
**		-1	Syntax error error
*/

int WSRC_init()
{
    int p;
    for(p=0; par[p].name; p++) {	/* Clear out old values */
        if (par[p].value) {
	    free(par[p].value);
	    par[p].value = 0;
	}
    }
    state = beginning;
    return 0;
}

int WSRC_treat(char c)
{
    switch (state) {
    case beginning:
        if (c=='(') state = before_tag;
	break;
	
    case before_tag:
        if (c==')') {
	    state = done;
	    make_address();
	    return 1;			/* Done with input file */
	} else if (c==':') {
	    param_count = 0;
	    state = colon;
	}				/* Ignore other text */
	break;

    case colon:
        if (WHITE(c)) {
	    param[param_count++] = 0;	/* Terminate */
	    for(param_number = 0; par[param_number].name; param_number++) {
		if (0==strcmp(par[param_number].name, param)) {
		    break;
		}
	    }
	    if (!par[param_number].name) {	/* Unknown field */
	        if (TRACE) fprintf(stderr,
		    "WAISGate: Unknown field `%s' in source file\n",
		    param);
		state = before_tag;	/* Could be better ignore */
		return 0;
	    }
	    state = before_value;
	} else {
	    if (param_count < PARAM_MAX)  param[param_count++] = c;
	}
	break;
	
    case before_value:
        if (c==')') {
	    state = done;
	    make_address();
	    return 1;			/* Done with input file */
	}
	if (WHITE(c)) return 0;		/* Skip white space */
	param_count = 0;
	if (c=='"') {
	    state = quoted_value;
	    break;
	}
	state = (c=='"') ? quoted_value : value;
	param[param_count++] = c;	/* Don't miss first character */
	break;

    case value:
        if (WHITE(c)) {
	    param[param_count] = 0;
	    StrAllocCopy(par[param_number].value, param);
	    state = before_tag;
	} else {
	    if (param_count < PARAM_MAX)  param[param_count++] = c;
	}
	break;

    case quoted_value:
        if (c=='"') {
	    param[param_count] = 0;
	    StrAllocCopy(par[param_number].value, param);
	    state = before_tag;
	} else {
	    if (param_count < PARAM_MAX)  param[param_count++] = c;
	}
	break;
	
    case done:				/* Ignore anything after EOF */
        make_address();
	return 1;

    } /* switch state */
    return 0;
}


/*	Output equivalent HTML
**	----------------------
**
** On exit,
**	returns	0	Ok, file exists
**		-1	Bad - not good source file parse.
*/

int WSRC_gen_html(void)

{
    char filename[256];
    FILE * html;
    struct stat buffer;		/* File status buffer */
    int status;
    
/*	Check whether a copy of this file already exists
*/
    if (!(par[PAR_IP_NAME].value &&
	par[PAR_TCP_PORT].value &&
	par[PAR_DATABASE_NAME].value)) return -1;	/* Can't */
 
    make_address();
    sprintf(filename, "%s%s:%s:%s.html",
    		    WAIS_CACHE_ROOT,
		    par[PAR_IP_NAME].value,
		    par[PAR_TCP_PORT].value,
		    www_database);

    status = stat(filename, &buffer);
    if (status==0) {
	if (buffer.st_mtime > time(0) - CACHE_PERIOD)
	    return 0;	/* Ok, recent file already exists */
    }

/*	If file does not exist, write it.
*/
    html = fopen(filename, "w");
    if (!html) {
        if (TRACE) fprintf(stderr, "Can't open file `%s' for write!\n",
		filename);
	return -1;
    }
    sprintf(WSRC_address, "/%s:%s/%s",
		    par[PAR_IP_NAME].value,
		    par[PAR_TCP_PORT].value,
		    www_database);
    
    fprintf(html, "<TITLE>%s index</TITLE>\n",
		    par[PAR_DATABASE_NAME].value);
    fprintf(html, "<H1>%s</H1>\n",
		    par[PAR_DATABASE_NAME].value);
    fprintf(html, "<XMP>%s\n", par[PAR_DESCRIPTION].value);
    fprintf(html, "</XMP>\n");
    fprintf(html, "This database is maintained by %s on host %s.\n",
	    par[PAR_MAINTAINER].value, par[PAR_IP_NAME].value);
    fclose(html);
    
    free(www_database);
    return 0;
    
} /* WSRC_gen_html() */
    

/*			Main Program
**			============
**
** Takes .src on stdin and makes cache file.
*/

#ifdef TEST
PUBLIC int WWW_TraceFlag = 0;
int main(int argc, char *argv[])
{
    char c;
    
    if (argc>1) 
        WWW_TraceFlag = 0==strcmp(argv[1], "-v") ? 1 : 0;
    
    for(WSRC_init(); (c=getc(stdin)) != EOF; ) {
	if (WSRC_treat(c)) break;
        /* fprintf(stderr, "char '%c', state = %d\n", c, state); */
    }
    WSRC_gen_html();
        
    return 0;
    
} /* main */
#endif
