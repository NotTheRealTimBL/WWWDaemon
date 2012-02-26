/* 		Create a HTML document representation of a directory
**		====================================================
**
**	Created August 1992 by CTB
**
*/

#include <sys/stat.h>
#include <sys/dir.h>
#include "HTUtils.h"
#include "HTDirRead.h"

#include "HTTCP.h"
#include "tcp.h"
#include "HTParse.h"

/* #include "HTTPVersion.h" */

#define GOT_READ_DIR 1    /* if directory reading functions are available */

/* In HTDaemon module: */

extern HTWriteASCII PARAMS((int soc, const char * message)); 
extern HTSendPreformatted PARAMS((int dest, int source));

PUBLIC int HTDirAccess = HT_DIR_SELECTIVE;	/* Directory access level */
PUBLIC int HTDirReadme = HT_DIR_README_TOP;	/* Where to put README */


/*	Send README file
**
**  If a README file exists, then it is inserted into the document here.
*/
PRIVATE void do_readme ARGS2(int, soc, CONST char *, localname)
{ 
    int fd;
    char * readme_file_name = 
	malloc(strlen(localname)+ 1 + strlen(HT_DIR_README_FILE) + 1);
    strcpy(readme_file_name, localname);
    strcat(readme_file_name, "/");
    strcat(readme_file_name, HT_DIR_README_FILE);
    fd=open(readme_file_name,  O_RDONLY, 0);
    if (fd >= 0) {
	HTSendPreformatted(soc, fd);
	close(fd);
    } 
}
/* 	Read directory file and output HTML
**	-----------------------------------
**
**	on entry: addr is the name of the directory file
**		soc is the socket to which the data is to be sent
**
**	On exit: returns YES if the message was sent successfully, NO otherwise
**
**	The procedure works by reading the directory file and creating a HTML
**	anchor for each entry, so that the client can link to any entry in the
**	directory.
**
** On entry,
**	addr		The address of the directory to be read
**	soc		The socket connected to the client
**	version		Unused at present
*/
PUBLIC BOOL DirToHTML ARGS3(char *, addr, int, soc, char *, version)
{

#ifdef GOT_READ_DIR

    char * localname = NULL;
    
    DIR *dp;
    struct direct * dirbuf;


#ifdef OLD_CODE
    struct sockaddr soc_addr;
    SockA * ttm = (SockA *)(& soc_addr);
    char *host_addr;
    char port[5];
#endif

    char * tmpstr=NULL;
    char * tmpfilename = NULL;
    char * shortfilename = NULL;
    CONST char * message = "unknown error!!";
    struct stat file_info;

#ifdef OLD_CODE
    getsockname(soc,&soc_addr,&addrlen);
    host_addr=HTInetString(&soc_addr); /* get internet address of this machine */	
    sprintf(port,"%hu",ttm->sin_port); /* get port currently being used */
#endif
	
    StrAllocCopy(localname,addr);	    /* hackable copy */

    if (HTDirAccess == HT_DIR_FORBID) {
	message = "directory access if forbidden";
	goto error;		/* What is this, BASIC?! */
    } 

/*	If the document name end in slash, this causes confusion
**	to the client if we use relative pathnames. Let's not do that.
*/
/*	In fact, let's forbid access to the root directory anyway,
**	it sounds dangerous, and simplifies lif to forbid it.
*/
    if (localname[strlen(localname)-1]=='/') {
	message = "document names can't end in `/'";
	goto error;
    }
 
 
 if (TRACE) fprintf(stderr,"HTDirRead: %s is a directory\n", localname);

#ifdef SUPRESS_THIS
 if (!strcmp(version,"0.9")) { 				/* old client */
     StrAllocCat(tmpstr,"");
 }
 else { 						/* new client */
      time_t now;
      char ltmp[100];
      StrAllocCopy(tmpstr,"RESPONSE=\"DATA\" HTTPV=\"");
      StrAllocCat(tmpstr,HTTPVERSION);
      StrAllocCat(tmpstr,"\"\r\nNOTATION=\"HTML\" TOEOF\r\nFETCHED=\"");
      now = time(NULL);
      strftime(ltmp,100,"%d %m %y %X", localtime(&now));
      StrAllocCat(tmpstr,ltmp);
      StrAllocCat(tmpstr,"\"\r\nENDHEADER\r\n");
 }
#endif

/*	Selective access means only thos directories containing a
**	marker file can be browsed
*/
    if (HTDirAccess == HT_DIR_SELECTIVE) {
        char * enable_file_name = 
	    malloc(strlen(localname)+ 1 + strlen(HT_DIR_ENABLE_FILE) + 1);
	strcpy(enable_file_name, localname);
	strcat(enable_file_name, "/");
	strcat(enable_file_name, HT_DIR_ENABLE_FILE);
	if (stat(enable_file_name, &file_info) != 0) {
	    message = "selective access is not enabled for this directory";
	    goto error;
	}
    }

    
    StrAllocCat(tmpstr,"<HEAD>\r\n<TITLE>");
    StrAllocCat(tmpstr,addr);
    StrAllocCat(tmpstr," directory</TITLE>\r\n</HEAD>\r\n<BODY>\r\n<H1>");			
    
    /* in_root = (strcmp(localname, "/") == 0); */
    shortfilename=strrchr(localname,'/');
	/* put the last part of the path in shortfilename */
/* if (!in_root) */
	shortfilename++;               /* get rid of leading '/' */
		    
    StrAllocCat(tmpstr,shortfilename);
    StrAllocCat(tmpstr,"</H1>\r\n");
    HTWriteASCII(soc,tmpstr);
		
/*	The README options allow text to be automatically
**	inserted at the top or bottom of the page
*/
    if (HTDirReadme == HT_DIR_README_TOP) do_readme(soc, localname);

    dp = opendir(localname);
    if (!dp) {	/* unreadable */
        message = 
	"the server can't read the directory (may be read protected)";
	goto error;			    

     } else {  /* if the directory file is readable */
      	char * endbit;

        HTWriteASCII(soc, "<DIR>\r\n");
      	while (dirbuf = readdir(dp)) {
			/* while there are directory entries to be read */
         if (dirbuf->d_ino == 0) 
		             /* if the entry is not being used, skip it */
	     continue;
			
	 if (!strcmp(dirbuf->d_name,"."))
	     continue;      /* skip the entry for this directory */
			    
	 if (strcmp(dirbuf->d_name,".."))
				 /* if the current entry is parent directory */
	     if ((*(dirbuf->d_name)=='.') || (*(dirbuf->d_name)==','))
		 continue;    /* skip those files whose name begins
						with '.' or ',' */
    
         if ((HTDirReadme != HT_DIR_README_NONE)
	 &&  (strcmp(dirbuf->d_name, HT_DIR_README_FILE)==0))
	 	continue;	/* No sense in listin it, they've seen it */
		
	 StrAllocCopy(tmpfilename,localname);
	 if (strcmp(localname,"/")) 
					/* if filename is not root directory */
	     StrAllocCat(tmpfilename,"/"); 
	 else
	     if (!strcmp(dirbuf->d_name,".."))
		 continue;
		/* if root directory and current entry is parent
					directory, skip the current entry */
			
	 StrAllocCat(tmpfilename,dirbuf->d_name);
			/* append the current entry's filename to the path */
	 HTSimplify(tmpfilename);
		
	 HTWriteASCII(soc,"<LI>");
	 HTWriteASCII(soc,"<A HREF=\"");
	 HTWriteASCII(soc, shortfilename);
	 HTWriteASCII(soc,"/");
	 HTWriteASCII(soc,dirbuf->d_name);
	 HTWriteASCII(soc,"\">");
	 	
	 stat(tmpfilename, &file_info);
			
	 if (strcmp(dirbuf->d_name,"..")) {
/* if the current entry is not the parent directory then use the file name */
             HTWriteASCII(soc,dirbuf->d_name);
	     if (((file_info.st_mode) & S_IFMT) == S_IFDIR)
	         HTWriteASCII(soc,"/"); 
	 }		        
	 else {
			/* use name of parent directory */
     	    endbit = strrchr(tmpfilename, '/');
	    HTWriteASCII(soc, "Up to ");
	    HTWriteASCII(soc, endbit &&(*endbit+1)?endbit+1:tmpfilename);
	}	
	HTWriteASCII(soc,"</A>\r\n");
     } /* end while directory entries left to read */
     HTWriteASCII(soc,"</DIR>\r\n");	    

/*	The README options allow text to be automatically
**	inserted at the top or bottom of the page
*/
     
     closedir(dp);
    free(tmpfilename);
		    
    } /* end if directory readable */	 
		
    if (HTDirReadme == HT_DIR_README_BOTTOM) do_readme(soc, localname);

     HTWriteASCII(soc,"</BODY>\r\n");	    

    free(localname);
    return (YES);


/*	Jump here to exit gracefully on error
**	-------------------------------------
*/
error:
    if (TRACE) fprintf(stderr, "HTDirRead: Error returned for `%s':\n   %s.\n",
    	localname, message);
	
    HTWriteASCII(soc, 
    "<BODY>\r\n<h2>HTTP Server Error</h2>\r\nThe HTTP server reports that the document\r\n");
    HTWriteASCII(soc, localname);
    HTWriteASCII(soc, "\r\ncould not be accessed because\r\n");
    HTWriteASCII(soc, message);
    HTWriteASCII(soc, ".\r\n</BODY>\r\n");
    free(localname);
    return NO;

#else		/* Not GOT_READ_DIR */
    return (NO);
#endif

}