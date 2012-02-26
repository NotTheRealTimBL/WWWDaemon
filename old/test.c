/*		Test Gateway Code
**
** This code simulates a daemon program. The first argument is the
** document address, the second is teh keywords.
*/
#include "HTUtils.h"
#include <stdio.h>

#ifdef __STDC__
extern int HTRetrieve(char * arg, char * keywords, int soc);	/* Handle one request */
#else
extern int HTRetrieve();	/* Handle one request */
#endif

PUBLIC char HTClientHost[16];	/* Peer internet address */

/*	Program-Global Variables
**	------------------------
*/

PUBLIC int    WWW_TraceFlag=1;	/* Control flag for diagnostics */
PUBLIC FILE * logfile = stderr;	/* Log file if any  */
PUBLIC char * log_file_name = "/dev/tty";/* Log file name if any (WAIS code) */


int main(int argc, char*argv[])
{
    int status;
    if (TRACE) printf('Trace on\n");
    status = HTRetrieve(argv[1], argv[2], 1);
    printf("HTRetrieve returned %d\n", status);
    
} /* main */