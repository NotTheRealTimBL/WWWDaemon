/* 		Create a HTML document representation of a directory
**		====================================================
**
**	Created August 1992 by CTB
**
*/


#ifndef HTMLDIR
#define HTMLDIR

#include "HTUtils.h"


/* 	Read directory file and output HTML
**	-----------------------------------
**
**	on entry: addr is the name of the directory file
**		soc is the socket to which the data is to be sent
**
**	On exit: returns YES if the message was sent successfully, NO otherwise
*/
extern BOOL DirToHTML PARAMS((char * addr, int soc, char * version));


/*	Controlling globals
*/
extern int HTDirAccess;		/* Directory access level */

#define HT_DIR_FORBID 		0	/* Altogether forbidden */
#define HT_DIR_SELECTIVE	1	/* If .www_browsable exists */
#define HT_DIR_OK		2	/* Any accesible directory */

#define HT_DIR_ENABLE_FILE	".www_browsable" /* If exists, can browse */

extern int HTDirReadme;		/* Include readme files? */
				/* Values: */
#define HT_DIR_README_NONE	0
#define HT_DIR_README_TOP	1
#define HT_DIR_README_BOTTOM	2

#define HT_DIR_README_FILE		"README"

#endif
