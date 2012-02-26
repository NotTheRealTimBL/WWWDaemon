/*		Handle HTTP request for CERNVM FIND server	FINDGate.c
**
**	This code takes a document name and converts it into
**	a VM CMS command to produce the hypertext the caller requires.
**	This involves, in general, calling an EXEC file which puts
**	the data onto the "stack" The stack allows the data to be read from
**	stdin as though it came from the command device.
**	The commands are written to return a value which is equal to the
**	number of lines put onto the stack.
**
** History
**	29 May 91	Separated from daemon program - TBL
**	19 May 91	Retrieve subroutine format used. -TBL
*/

#define BUFFER_SIZE 4096	/* Arbitrary size for efficiency */

#include "HTUtils.h"
#include "tcp.h"		/* The whole mess of include files */
#include "HTTCP.h"		/* Some utilities for TCP */

extern int WWW_TraceFlag;	/* Control diagnostic output */
extern FILE * logfile;		/* Log file output */
extern char HTClientHost[16];	/* Client name to be output */
extern int HTWriteASCII(int soc, const char * s);	/* In HTDaemon.c */



/*	Strip white space off a string
**
** On exit,
**	Return value points to first non-white character, or to 0 if none.
**	All trailing white space is overwritten with zero.
*/
PRIVATE char * strip(char * s)
{
#define SPACE(c) ((c==' ')||(c=='\t')||(c=='\n')) 
    char * p=s;
    for(p=s;*p;p++);		        /* Find end of string */
    for(p--;p>=s;p--) {
    	if(SPACE(*p)) *p=0;	/* Zap trailing blanks */
	else break;
    }
    while(SPACE(*s))s++;	/* Strip leading blanks */
    return s;
}



/*	Handle one message
**	------------------
**
** On entry,
**	soc		A file descriptor for input and output.
** On exit,
**	returns		>0	Channel is still open.
**			0	End of file was found, please close file
**			<0	Error found, please close file.
*/
/*	Retrieve information
**	--------------------
*/
#ifdef __STDC__
int HTRetrieve(const char * arg, const char * keys, int soc)
#else
int HTRetrieve(arg, keys, soc)
    char *arg;
    char *keys;
    int soc;
#endif
{
#define COMMAND_SIZE	255
#define MAX_LINES 10000			/* Maximum number of lines returned */
    char command[COMMAND_SIZE+1];
    char keywords[COMMAND_SIZE+1];
    char argument[COMMAND_SIZE+1];
    char buffer[BUFFER_SIZE];
    char * filename;	/* Pointer to filename or group list*/
    char system_command[COMMAND_SIZE+1];
    int lines;		/* Number of lines returned by EXEC file */
   
/* 	Remove host and any punctuation. (Could use HTParse @)
*/      
    strcpy(argument, arg);
    filename = argument;
    if (argument[0]=='/') {
	if (argument[1]=='/') {
	    filename = strchr(argument+2, '/');	/* Skip //host/ */
	    if (!filename) filename=argument+strlen(argument);
	} else {
	    filename = argument+1;	/* Assume root: skip slash */
	}
    }

    if (!*filename) {
        HTWriteASCII(soc,"<P>Error: No document ID provided.<P>\n");
	return 0;

    }
    
    if (keys) strcpy(keywords, keys);
    else keywords[0] = 0; /* As if just "?" given (historical) */
    
/*	Formats supported by FIND are:
**
**	/FIND				Help for find general index
**	/FIND/				(same)
**	/FIND/?				(same)
**	/FIND/group.disk.ft.fn		Get a document from a disk- NOT INDEX
**	/FIND/grouplist?		Help for group index
**	/FIND/grouplist?keylist		Search a set of disks for keywords
**					where grouplist = group+group+group.. or void
**					keylist = key+key+key... (not void)
*/
    if((
		(0==strncmp(filename,"FIND",4))
		|| (0==strncmp(filename,"find",4))
	) && (
	    (filename[4]==0)
	    || (filename[4]=='/')
	    || (filename[4]=='?')
	)) {
    
	filename=filename+4;
	if (*filename=='/') filename++;	/* Skip first slash */
	    
	if (keys || !*filename) {		/* Index access */
	    char *p;
	    while((p=strchr(filename,'+'))!=0) *p=' ';/* Remove +s */
	    while((p=strchr(keywords,'+'))!=0) *p=' ';/* Remove +s */
	    
	    if (!*keywords) {			/* If no keywords    */
		HTWriteASCII(soc, "<IsIndex><TITLE>");
		HTWriteASCII(soc, filename);
		HTWriteASCII(soc, " keyword index</TITLE>\n");
		if (*filename) {
		    HTWriteASCII(soc, "This index covers groups: `");
		    HTWriteASCII(soc, filename);
		    HTWriteASCII(soc, "'");
		} else {
		    HTWriteASCII(soc,
"This is the general CERN Computer Center public document index");
		}
		HTWriteASCII(soc,
". Your words will be matched against author-supplied keywords.\n");
		HTWriteASCII(soc,
"<P>Supply keywords to search for information.<P>\n");
		return 0;
	    }

	    sprintf(buffer, "<IsIndex>\n<TITLE>%s in %s</TITLE>\n",
	    	keywords, *filename ? filename : "general index" );
	    HTWriteASCII(soc, buffer);
	    
	    sprintf(system_command,"EXEC FSEARCH %s %s",
	    	HTClientHost, keywords);
	    if (*filename) {
		strcat(system_command, " ( ");
		strcat(system_command, filename);
	    }
	} else {
	    sprintf(system_command,"EXEC FGET %s %s",
	    	HTClientHost, filename);
	}
	
    }  /* if FIND */

/*	Formats supported by NEWS are:
**
**	/NEWS				General news
**	/NEWS/grouplist			News for given groups
**	/NEWS/grouplist?		All news for all given groups
**	/NEWS/grouplist?keylist		Search news for keywords
**					where grouplist = group+group+group.. or void
**					keylist = key+key+key... (can be void)
*/
    else if((
		(0==strncmp(filename,"NEWS",4))
		|| (0==strncmp(filename,"news",4))
	    ) && (
		(filename[4]==0)
		|| (filename[4]=='/')
		|| (filename[4]=='?')
	    )) {
    
	filename=filename+4;
	if (*filename=='/') filename++;		/* Skip first slash */

	{
	    char *p;
	    while((p=strchr(filename,'+'))!=0) *p=' ';/* Remove +s */
	    while((p=strchr(keywords,'+'))!=0) *p=' ';/* Remove +s */
	    
	    sprintf(system_command,"EXEC FSEARCH %s ", HTClientHost);
	    if (keywords) strcat(system_command, keywords);
	    strcat(system_command, " ( NEWS ");
	    if (*filename) {
		strcat(system_command, "(");
		strcat(system_command, filename);
		strcat(system_command, ") ");
	    }
	}

	sprintf(buffer, "<IsIndex>\n<TITLE>%s NEWS in %s</TITLE>\n",
	    keywords, filename ? filename : "general index" );
	HTWriteASCII(soc, buffer);
    } /* NEWS */
	
/*	Formats supported by WHO are:
**
**	/WHO					Cover page
**	/WHO/s=surname;g=first;ou=div;		Details on a person
**	/WHO/s=surname;g=first;ou=div;ph=nnn;	Details on a person
**	/WHO?keylist				Search phonebook for keywords
*/
    else if((
		(0==strncmp(filename,"WHO",3))
		|| (0==strncmp(filename,"who",3))
	    ) && (
		(filename[3]==0)
		|| (filename[3]=='/')
	    )) {
 	
#ifdef BUILT_IN_COVER_PAGE	/* now provided by WHOSERV EXEC */
	if (!*filename && !keys) {		/* No info asked for */
	
HTWriteASCII(soc,
"<IsIndex><title>CERN telephone and electonic mail directory</title>\n");
HTWriteASCII(soc,  "<h1>Phone Book</h1>\n" );
HTWriteASCII(soc, "This is an index of people and phone numbers at CERN.\n");
HTWriteASCII(soc, 
"Provide a surname and/or given name, or phone number as a keyword.\n");
HTWriteASCII(soc, "<p>See also:<UL>");
HTWriteASCII(soc,  "<LI>About searching individual <a \n");
HTWriteASCII(soc,  "href=http://info.cern.ch/hypertext/DataSources/WHO/FieldNames.html>\n");
HTWriteASCII(soc,  "fields</a>.\n");
HTWriteASCII(soc,  "<LI><a href=/FIND/PUB.INFO.HELPINFO.WHO_PROB(X/G/H)>");
HTWriteASCII(soc,  "what to do if the data is incorrect</a>,\n");
HTWriteASCII(soc, "<LI><a href=/FIND/yellow?>'Yellow pages'</a> \n");
HTWriteASCII(soc,"(ou <a href=/FIND/jaune?>'Pages jaunes'</a> en francais)\n");
HTWriteASCII(soc,  "for an index of functions rather than names,\n");
HTWriteASCII(soc,  "</UL>\n");
HTWriteASCII(soc,  "\n");
	    return 0;
	}
#endif
	    
	if (keys) sprintf(system_command,"EXEC WHOSERV %s %s?%s",
	    HTClientHost, filename, keys);
	else sprintf(system_command,"EXEC WHOSERV %s /%s",
	    HTClientHost, filename);
		
	    
/*	Pick up other unknown forms
*/	
    } else { /* not FIND or NEWS */
	HTWriteASCII(soc, "Bad syntax. No such document.\n");
	return 0;
    }

/*	Execute system command and get the results
*/
    lines = system(system_command);	/* Number of stacked lines */
    if (TRACE) printf("Command `%s' returned %i lines.\n",
		system_command, lines);
    if ((lines<=0)||(lines>=20000)) {
	sprintf(buffer,
	"\nSorry, the server executing command\n  %s\n  returned code %d\n", 
			system_command, lines);
	HTWriteASCII(soc, buffer);
	return 0;
    }
    
    /*	Copy the file across: */
    
    for(;lines; lines--){		/* Speed necessary here */
	char *p;
	if (!fgets(buffer, BUFFER_SIZE, stdin))
	    sprintf(buffer,
 "*** End of file with %d lines left after command\n%s\n - mail BERND@CERNVM",
	    lines, system_command);
	for(p=buffer; *p; p++) *p = TOASCII(*p);
	write(soc, buffer, p-buffer);
    }
    return 0;
	
} /* HTRetrieve() */

