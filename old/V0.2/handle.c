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

#include "HTUtils.h"
#include "tcp.h"
#ifdef RULES			/* Use rules? */
#include "HTRules.h"
#endif
#include "HTParse.h"

extern int WWW_TraceFlag;	/* Control diagnostic output */
extern FILE * LogFile;		/* Log file output */
extern char HTClientHost[16];	/* Client name to be output */


/*		Read a file
**		-----------
**
**	The function copies a file given a source and destination file number.
**	The destination may be a network socket.
**
** On exit,
**	returns		0 for end of file, <0 for error.
*/
PRIVATE int read_file(int dest, int source)
{
   unsigned int status;
   char		    buffer[BUFFER_SIZE];
   for(;;) {
       status = read(source, buffer, BUFFER_SIZE);
       if (!status) return -1;
       (void) NETWRITE(dest, buffer, status);
   }
}


/*	Retrieve a file
**	---------------
*/
#ifdef __STDC__
HTRetrieve(const char * arg, const char * keywords, int soc)
#else
HTRetrieve(arg, soc)
    char *arg;
    char *keywords;
    int soc;
#endif
{
    char * filename;	/* Pointer to filename or group list*/
    int fd;		/* File descriptor number */

    if (keywords) {
	if (TRACE) printf("HTHandle: can't perform search %s\n",
		arg);
	HTWriteASCII(soc,
	    "Sorry, this server does not perform searches.\n");
#ifdef RULES
	free(arg);
#endif
	return fd;
    }
    
    HTSimplify(arg);	/* Remove ".." etc  (DMX) */

#ifdef RULES
    arg = HTTranslate(arg);
    if (!arg) {
	HTWriteASCII(soc,
	    "Document address invalid or access not authorised\n");
	if (LogFile) {
	    fprintf(LogFile, "** SECURITY FAIL FOR ABOVE **\n");
	    fflush(LogFile);
	}
	if (TRACE) printf( "HTHandle: Security fail ***\n");
	return fd;
    }
#endif

/* Remove host and any punctuation. (Could use HTParse to remove access too @)
*/      
    filename = arg;
    if (arg[0]=='/') {
	if (arg[1]=='/') {
	    filename = strchr(arg+2, '/');	/* Skip //host/ */
	    if (!filename) filename=arg+strlen(arg);
	} else {
	    filename = arg+1;	/* Assume root: skip slash */
	}
    }

    if (*filename) {	/* (else return test message) 	*/
	fd = open(arg, O_RDONLY, 0 );	/* un*x open @@ */
	if (fd<0) {
	    if (TRACE) printf("HTHandle: can't open file %s\n",
		    arg);
	    HTWriteASCII(soc,
	        "Document address invalid or access not authorised\n");
#ifdef RULES
	    free(arg);
#endif
	    return fd;
	}
	status = read_file(soc, fd);
	close(fd);
#ifdef RULES
	free(arg);
#endif
	return 0;
    } /* Filename exists */

#ifdef RULES
    free(arg);
#endif
}	/* Retrieve */

