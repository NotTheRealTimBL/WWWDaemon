/*		Handle HTTP request for CERNVM FIND server	vmhandle.c
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
*/
#include "HTUtils.h"
#include "tcp.h"		/* The whole mess of include files */
#include "tcputi.h"		/* Some utilities for TCP */

extern int WWW_TraceFlag;	/* Control diagnostic output */
extern FILE * LogFile;		/* Log file output */

/*		Read a file
**		-----------
**
**	The function reads a file from the given file POINTER
**	into the given file DESCRIPTOR!
**
** On exit,
**	returns		0 for end of file, <0 for error.
*/
PRIVATE int read_file(int dest, FILE * source)
{
   unsigned char * status;
   char		    buffer[BUFFER_SIZE];
   for(;;) {
       status = (unsigned char *)fgets(buffer, BUFFER_SIZE, source);
       if (!status) return -1;
       (void) write(dest, buffer, status);
   }
}


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


/*	Send a string down a socket
**	---------------------------
**
**	The trailing zero is not sent.
**	There is a maximum string length.
*/
PRIVATE int write_ascii(int soc, char * s)
{
    char * p;
    char ascii[255];
    char *q = ascii;
    for (p=s; *p; p++) {
        *q++ = TOASCII(*p);
    }
    return write(soc, ascii, p-s);
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
int handle(int soc)
{
#define COMMAND_SIZE	255
#define MAX_LINES 10000			/* Maximum number of lines returned */
    char command[COMMAND_SIZE+1];
    char		    buffer[BUFFER_SIZE];
    int	status;
    char * arg;		/* Pointer to the argument string */
    char * filename;	/* Pointer to filename or group list*/
    char * keywords;	/* pointer to keywords */
    char system_command[COMMAND_SIZE+1];
    int lines;		/* Number of lines returned by EXEC file */
    int fd;		/* File descriptor number */
    int	isIndex = 0;	/* Is the thing asked for an index? */
   
    for (;;) {
	status = read(soc, command, COMMAND_SIZE);
	if (status<=0) {
	    if (TRACE) printf("read returned %i, errno=%i\n", status, errno);
	    return status;	/* Return and close file */
	}
	command[status]=0;	/* terminate the string */
#ifdef VM
	{
	    char * p;
	    for (p=command; *p; p++) {
		*p = FROMASCII(*p);
		if (*p == '\n') *p = ' ';
	    }
	}
#endif
	if (TRACE) printf("read string is `%s'\n", command);

	arg=strchr(command,' ');
	if (arg) {
	    *arg++ = 0;				/* Terminate command 	*/
	    arg = strip(arg);			/* Strip space 		*/
	    if (0==strcmp("GET", command)) {	/* Get a file 		*/

/* 	Remove host and any punctuation. (Could use HTParse to remove access too @)
*/      
		filename = arg;
		if (arg[0]=='/') {
		    if (arg[1]=='/') {
		        filename = strchr(arg+2, '/');	/* Skip //host/ */
			if (!filename) filename=arg+strlen(arg);
		    } else {
		        filename = arg+1;		/* Assume root: skip slash */
		    }
		}

		if (*filename) {		/* (else return test message) 	*/
		    keywords=strchr(filename, '?');
		    if (keywords) *keywords++=0;	/* Split into two */
#ifdef VM

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
			   || (filename[4]='?')
			)) {
		    
		        filename=filename+4;
			if (*filename=='/') filename++;	/* Skip first slash */
			 
			if (!*filename) {	 /* If no data at all,    */
			    if (!keywords)
			     keywords = filename; /* Respond as if just "?" given */			
			}
    
			if (keywords) {
			    char *p;
			    while((p=strchr(filename,'+'))!=0) *p=' ';/* Remove +s */
			    while((p=strchr(keywords,'+'))!=0) *p=' ';/* Remove +s */
			    
			    if (!*keywords) {	/* If no keywords    */
				write_ascii(soc, "<IsIndex><TITLE>");
				write_ascii(soc, filename);
				write_ascii(soc, " keyword index</TITLE>\n");
				if (*filename) {
				    write_ascii(soc, "This index covers groups: `");
				    write_ascii(soc, filename);
				    write_ascii(soc, "'");
				} else {
			    	    write_ascii(soc,
		"This is the general CERN public document index");
				}
				write_ascii(soc,
		".<P>You may use your browser's keyword search on this index.<P>\n");
				return 0;
			    }
    
			    strcpy(system_command,"EXEC NICFIND1 ");
			    strcat(system_command, keywords);
			    if (*filename) {
				strcat(system_command, " ( ");
				strcat(system_command, filename);
			    }
			    isIndex = 1;
			} else {
			    strcpy(system_command,"EXEC NICFOUN1 ");
			    strcat(system_command, filename);
			    isIndex = 0;
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
				|| (filename[4]='?')
			    )) {
		    
			isIndex = 1;
		        filename=filename+4;
			if (*filename=='/') filename++;		/* Skip first slash */
			 
			if (!*filename) {	 /* If no data at all,    */
			    if (!keywords)
			     keywords = filename; /* Respond as if just "?" given */			
			}
			
			if (!keywords) keywords = ""; /* "If nun, write none"-1066 */
			
			{
			    char *p;
			    while((p=strchr(filename,'+'))!=0) *p=' ';/* Remove +s */
			    while((p=strchr(keywords,'+'))!=0) *p=' ';/* Remove +s */
			    
			    strcpy(system_command,"EXEC NICFIND1 ");
			    if (keywords) strcat(system_command, keywords);
			    strcat(system_command, " ( NEWS ");
			    if (*filename) {
				strcat(system_command, "(");
				strcat(system_command, filename);
				strcat(system_command, ") ");
			    }
			}
			
		    } else { /* not FIND or NEWS */
			write_ascii(soc, "Bad syntax. No such document.\n");
			return 0;
		    }

/*	Execute system command and get the results
*/
		    lines = system(system_command);
		    if (TRACE) printf("Command `%s' returned %i lines.\n",
			     system_command, lines);
		    if (lines<=0) {
			write_ascii(soc,
			"\nSorry, the FIND server could not execute\n  `");
			write_ascii(soc, system_command);
			write_ascii(soc, "'\n\n");
			return 0;
		    }
		    
		    /*	Copy the file across: */
		    
		    if (isIndex) write_ascii(soc, "<IsIndex>\n");
		    for(;lines; lines--){		/* Speed necessary here */
			char *p;
			fgets(buffer, BUFFER_SIZE, stdin);
	    /*	    if (strcmp(buffer,"<END_OF_FILE>")==0) break;	*/
			for(p=buffer; *p; p++) *p = TOASCII(*p);
			write(soc, buffer, p-buffer);
		    }
		    return 0;
			


#else
		    fd = open(arg, O_RDONLY, 0 );	/* un*x open @@ */
		    if (fd<0) return fd;
		    status = read_file(soc, fd);
		    close(fd);
		    return 0;
#endif
		}
	    }	
	}

	write_ascii(soc, "<h1>FIND Server v1.03</h1><h2>Test response</h2>\n");
	write_ascii(soc, "\n\nThe (unrecognised) command used was `");
	write_ascii(soc, command);
	write_ascii(soc, "'.\n\n");
	
	if (TRACE) printf("Return test string written back'\n");
	return 0;		/* End of file - please close socket */
    } /* for */
    
} /* handle */

