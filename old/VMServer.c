/*		Handle HTTP request for CERNVM WWW server	VMServer.c
**
**	This code takes a document name and converts it into
**	a VM CMS command to produce the hypertext the caller requires.
**
** History
**	29 May 91	Separated from daemon program - TBL
**	19 May 91	Retrieve subroutine format used. -TBL
**	21 Dec 92	Made generic version -- all specifics in EXEC file TBL
*/
/*	The server calls a program (typically W3SERV EXEC on VM)
**	which must provide the data. It is called with the command line
**	format
**	EXEC W3SERV hostname docid [keywords]
**
**	where
**		hostname 	is the IP address of the requesting host
**		docid		is the W3 address of the document
**		keywords	are search terms if an index is being serached
**
**	The W3SERV program produces its results in hypertext. See the W3
**	documentation for the format of the hypertext or look at some examples.
**	This data must be output to the stack.
**
** 	IT IS IMPORTANT that the W3SERV program returns the number of
**	lines it has written to the stack, as that is the only way that we
**	can know how many lines to pull back off.
*/

#define SERVER_COMMMAND		"EXEC W3SERVE"

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
	}
    }

    if (!*filename) {
        HTWriteASCII(soc,"599 Error: No document ID provided.<P>\r\n");
	return 0;
    }
    

/*	Reconstitute keywords for search if any
*/
    if (keys) strcpy(keywords, keys);
    else keywords[0] = 0; /* As if just "?" given (historical) */

    {
	char *p;
	while((p=strchr(keywords,'+'))!=0) *p=' ';/* Remove +s */
    }

    sprintf(system_command,"%s %s %s %s",
	    	SERVER_COMMAND, HTClientHost, filename, keywords);

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

