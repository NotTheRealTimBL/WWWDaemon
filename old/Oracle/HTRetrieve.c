/*		Handle a Retrieve request from a WWW client	HTRetrieve.c
**		===========================================
**
** Authors
**	TBL	Tim Berners-Lee, CERN, Geneva	timbl@info.cern.ch
**	DMX	Daniel Martin
**
** History:
**	29 Apr 91 (TBL)	Split from daemon.c
**	5 Sept 91 (DMX)	Added path simplification to prevent '..'ing to an 
**			uncorrect directory.
**			Added '\r' as space for telneting to socket.
**	10 Sep 91 (TBL)	Reject request and log if fails authorisation
*/

/* (c) CERN WorldWideWeb project 1990,91. See Copyright.html for details */

#define BUFFER_SIZE 4096	/* Arbitrary size for efficiency */
#define INFINITY 512		/* file name length @@ FIXME */

#include "HTUtils.h"
#include "HTFormat.h"
#include "tcp.h"
#include "WWW.h"
#ifdef RULES			/* Use rules? */
#include "HTRules.h"
#endif
#include "HTParse.h"

extern int WWW_TraceFlag;	/* Control diagnostic output */
extern FILE * logfile;		/* Log file output */
extern char HTClientHost[16];	/* Client name to be output */
extern int HTWriteASCII PARAMS((int soc, char * s));	/* In HTDaemon.c */


#ifdef vms
/*	Convert unix-style name into VMS name
**	-------------------------------------
**
** Bug:	Returns pointer to static -- non-reentrant
** This is a simplified version of vms_name in HTFile.c (local files only)
*/
PRIVATE char * vms_name (CONST char * fn)
{

/*	We try converting the filename into Files-11 syntax.
**	Here, we assume that the file is on our VMS node.
**	Files-11, VMS, VAX and DECnet
**	are trademarks of Digital Equipment Corporation. 
*/
    static char vmsname[INFINITY];	/* returned */
    char * filename = (char*)malloc(strlen(fn)+1);  /* Copy to hack */
    char * second;		/* 2nd slash */
    char * last;		/* last slash */
    
    strcpy(filename, fn);
    second = strchr(filename+1, '/');		/* 2nd slash */
    last = strrchr(filename, '/');	/* last slash */
        
    if (!second) {				/* Only one slash */
	sprintf(vmsname, "%s", filename + 1);
    } else if(second==last) {		/* Exactly two slashes */
	*second = 0;		/* Split filename from disk */
	sprintf(vmsname, "%s:%s", filename+1, second+1);
	*second = '/';	/* restore */
    } else { 				/* More than two slashes */
	char * p;
	*second = 0;		/* Split disk from directories */
	*last = 0;		/* Split dir from filename */
	sprintf(vmsname, "%s:[%s]%s", filename+1, second+1, last+1);
	*second = *last = '/';	/* restore filename */
	for (p=strchr(vmsname, '['); *p!=']'; p++)
	    if (*p=='/') *p='.';	/* Convert dir sep.  to dots */
    }
    free(filename);
    return vmsname;
}

#endif /* vms */

/*		Read a file
**		-----------
**
**	The function copies a file given a source and destination file number.
**	The destination may be a network socket.
**
** On exit,
**	returns		0 for end of file, <0 for error.
*/
PRIVATE int read_file ARGS2(
	int,	dest,
	int,	source)
{
   unsigned int status;
   char		    buffer[BUFFER_SIZE];
   for(;;) {
       status = read(source, buffer, BUFFER_SIZE);
       if (!status) return 0;			/* OK */
       (void) NETWRITE(dest, buffer, status);
   }
}

/*	Determine file format from file name
**	------------------------------------
**
** Note: This function is also in the browser file HTFile.c
** If it gets complicated, it should be shared. (HTFile.c has a lot of
** other stuff in!)
*/

PRIVATE HTFormat HTFileFormat ARGS1 (CONST char *,filename)
{
    CONST char * extension;
    for (extension=filename+strlen(filename);
	(extension>filename) &&
		(*extension != '.') &&
		(*extension!='/');
	extension--) /* search */ ;

    if (*extension == '.') {
	return    0==strcmp(".html", extension) ? WWW_HTML
		: 0==strcmp(".rtf",  extension) ? WWW_RICHTEXT
		: 0==strcmp(".txt",  extension) ? WWW_PLAINTEXT
		: 0==strcmp(".tar",  extension) ? WWW_BINARY
		: 0==strcmp(".hqx",  extension) ? WWW_BINARY
		: 0==strcmp(".Z",  extension) ? WWW_BINARY
		: WWW_PLAINTEXT;	/* Unrecognised : try plain text */
    } else {
	return WWW_PLAINTEXT;
    } 
}


/*	Retrieve a file
**	---------------
*/
#ifdef __STDC__
int HTRetrieve(const char * arg, const char * keywords, int soc)
#else
int HTRetrieve(arg, keywords, soc)
    char *arg;
    char *keywords;
    int soc;
#endif
{
    char * filename;	/* Pointer to filename or group list*/
    int fd;		/* File descriptor number */
    int status;		/* of file_read */
    char * arg2 = 0;	/* Simplified argument */

    if (keywords) {
	if (TRACE) printf("HTHandle: starting search %s\n",
		arg);
		getOracle(keywords,soc);
		
		/* 1) call Oracle with request,
		   2) do HTWriteASCII(soc, ...) for all returned data
		*/

/*	HTWriteASCII(soc,
	    "Sorry, this server does not perform searches.\n");
		 */
	return fd;
    }
    
    StrAllocCopy(arg2, arg);
    HTSimplify(arg2);	/* Remove ".." etc  (DMX) @@ No support for VMS syntax */

#ifdef RULES
    {
    	char * arg3 = arg2;
	arg2 = HTTranslate(arg3);
	free(arg3);
    }
    if (!arg2) {
	HTWriteASCII(soc,
	    "Document address invalid or access not authorised\n");
	if (logfile) {
	    fprintf(logfile, "** SECURITY FAIL FOR ABOVE **\n");
	    fflush(logfile);
	}
	if (TRACE) printf( "HTHandle: Security fail ***\n");
	return fd;
    }
#endif

/* Remove host part.
*/
    filename = HTParse (arg2, "", PARSE_PATH | PARSE_PUNCTUATION);
    free(arg2);

    if (*filename) {	/* (else return test message) 	*/
	HTFormat format =  HTFileFormat(filename);
    
#ifdef vms
/* Assume that the file is in Unix-style syntax if it contains a '/'
   after the leading one @@ */
        char * vmsname = strchr(filename + 1, '/') ?
	  vms_name(filename) : filename + 1;
	fd = open(vmsname, O_RDONLY, 0);
#else
	fd = open(filename, O_RDONLY, 0);
	if(TRACE) printf ("HTAccess: Opening `%s' gives %d\n",
			  filename, fd);
#endif

	if (fd<0) {
	    if (TRACE) printf("HTHandle: can't open file %s\n",
#ifdef vms
		    vmsname);
#else
		    filename);
#endif
	    HTWriteASCII(soc,
	        "Document address invalid or access not authorised\n");
	    free(filename);
	    return fd;
	}
	
	/* Tell the client which format is to be sent */
	if (format != WWW_HTML) {
	    if (format==WWW_PLAINTEXT) {
	        HTWriteASCII(soc, "<PLAINTEXT>\n");
	    } else {
	        HTWriteASCII(soc,
"Sorry, this document appears to be neither a plain text file nor hypertext file.\n");
	        HTWriteASCII(soc,
"The current version of www does not yet handle multiple formats. Watch this space...\n");
		free(filename);
		return fd;
	    }
	}
	status = read_file(soc, fd);
	close(fd);
	free(filename);
	return status;
    } /* Filename exists */

    if (TRACE) printf("HTHandle: No file name\n");
    HTWriteASCII(soc,"Document address invalid\n");
    free(filename);
    return -1;
}	/* Retrieve */

