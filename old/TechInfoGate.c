/* 		WWW  --> TechInfo Gateway
  
  
  Copyright November 1992 by the University of Pennsylvania.
  Copyright 1992, 1993 CERN

Authors:
	LAM	Linda Murphy, University of Pennsylvania
	TBL	Tim Berners-Lee, CERN

History:
	   Dec 92	WWW gateway made (TBL) 
	   Nov 92	Written as Gopher Gateway (LAM)
*/
/* 
	 Gateway from World-Wide Web  to TechInfo Services

   This is a one way gateway from WWW protocol to TI protocol:
   WWW client connects to this server and makes a request, this
   program connects to a TechInfo server and gets the appropriate
   information, sends the response to the client, and then closes 
   the connection.

   Request from WWW client              This program's response
   -----------------------              -----------------------

GET /                                   Local TechInfo Server's main menu, 
                                        WorldWide TechInfo, and,

GET /tihost:tiport/type/tinodeid         Results of Techinfo transaction

GET /tihost:tiport/type/tinodeid?word+word
                                         Results of Techinfo search

GET /doc/worldwide-techinfo              List of TechInfo servers

GET /doc/WWW-TI-Gateway                  Document -- about this server

	Note that the gateway will also accept a
   Examples:
   
  

   	Input:
GET /

   	Output:

<title>WorldWideWebTechInfo gateway</title>
<h1>Techinfo Gateway</h1>
Welcome to the WWW techinfo gateway. Information available
through this service is that served by servers running the
TechInfo protocol from MIT.
See also:
<MENU>
<LI><A HREF="/doc/WWW-TI-Gateway">About this gateway</A>
<LI><A HREF="/ti-srv.upenn.edu:9000/M/836">About PennInfo</A>
<LI><A HREF="/ti-srv.upenn.edu:9000/D/7182">Today's Pennsylvania weather</a>
<LI><A HREF="/doc/worldwide-techinfo">World Wide TechInfo</a>
</MENU>

	Input:

GET /doc/worldwide-techinfo

	Output:
<TITLE>World Wide TechInfo</TITLE>
<MENU>
<LI><A href="/tiserve.mit.edu:9000/M/0">MIT</A>
<LI><A href="/xantos.uio.no:9000/M/0">University of Oslo</A>
</MENU>

	Input:
GET /ti-srv.upenn.edu:9000/M/0

   	Output:

<TITLE>whatever the title of node 0 is
<ISINDEX>
<MENU>
<LI><A HREF="//ti-srv.upenn.edu:9000/M/836>About PennInfo</A>
<LI><A HREF="/ti-srv.upenn.edu:9000/D/7182">Today's Pennsylvania weather</a>
</MENU>

	Note:  The <ISINDEX> means that a search can be made by the user
	who is at this node.
	

   Input:

GET /ti-srv.upenn.edu:9000/D/7182

   Output:
<PLAINTEXT>
{ascii text document}


   Input:	(Is this a TI search? - Tim)
   
GET /ti-srv.upenn.edu:9000/M/120?Internet+Gopher

   Output:

<TITLE>"Internet Gopher" under (title of node 120)</TITLE>
<ISINDEX>
<MENU>
<LI><A HREF="/ti-srv.upenn.edu:9000/D/3568">
The Internet Gopher - What is it?
</A>
</MENU>


   This program is not standalone -- it's meant to be called from
   inetd.

*/


/* CODE STARTS HERE ---   */

#ifdef OLD_INCLUDES
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include "HTUtils.h"
#include "tcp.h"		/* All system specific file, net etc */
#endif

#include <ctype.h>

#define LOCALTI_SERVER      "penninfo-srv.upenn.edu"
#define LOCALTI_PORT          "9000"
#define LOCALTI_MAINMENU   "0"
#define DEBUGLOG "/usr/users/murphy/www_ti_gw.log" 
#define MSGFILE  "/usr/users/murphy/www_ti_gw.msg"
#define GWVERSION "WWW-TI_GATE:1.1"

/* Which host is the keeper of the TechInfo servers worldwide? */

#ifndef TISERVERS_HOST
#define TISERVERS_HOST  "tiserve.mit.edu"
#endif

#ifndef TISERVERS_PORT
#define TISERVERS_PORT  "9000"
#endif

#define FLUSHLINES 10		/* Flush menus every 10 lines */

PUBLIC FILE * logfile = 0;	/* Log file if any */
PUBLIC char *HTClientHost;	/* Peer internet address */


/*	Special document IDs
*/

#define ABOUT_GW      "/WWW-TI-Gateway"      /* About this gopher */
#define TERMBASEDTI   "/telnet-techinfo"     /* Telnet session to techinfo */
#define WORLDWIDETI   "/worldwide-techinfo"  /* List of all TechInfo servers */

/*	Forward declarations
*/
PRIVATE int send_msg();
PRIVATE void logdebug();
PRIVATE void send_version();
PRIVATE int get_msg();


/*		Module-wide Variables
*/
PRIVATE FILE * client;

int gophinsock = -1;

char tibuf[200000];
char clientbuf[10000];


#define CR '\r'
#define LF '\n'
#define EOM             ".\r\n"
#define EOM_LEN         (sizeof(EOM) -1)
#define	terminator(str)	(!strncmp(str, EOM, EOM_LEN))

#ifndef RUNTIME_UID
#define RUNTIME_UID 1   /* 1 is usually the DAEMON */
#endif

/* TechInfo Protocol */
#define TICMD_GETSERVERS        "m"
#define TICMD_GETMENU           "w:2:%s:1"
#define TICMD_GETDOC            "t:%s:%d:%d"
#define TICMD_KEYSRCH           "b:%s"
#define TICMD_KEYSRCH_NODEID    "b:%s:%s"
#define TICMD_VERSION           "v:%s"
#define TIOKAY                  "0:"
#define TIMAXFIELDS             40
#define TIDELIM                 ':'
#define TOTCHARSTR              "Total Characters :"

/* Intermediary protocol -- definitions of urls for this gateway  */
#define I_DELIM         '/'
#define I_DOC           'D'
#define I_MENU          'M'


/*	Read a line from given socket
**
**  On entry,
**	line	contains enough space
**	sz	size of the line array
**	*idx	points to start of space
**	sock	is already connected and open for read
**
** On exit,
**	return -40	error
**		0	OK
**	*idx		points to terminating zero in line
**	line		conatins new data
*/
PRIVATE int readline (char *line, int sz, int sock, int *idx)
{
  int rc;

  *idx = 0;
  line[*idx] = 0;
  do {
    rc = read(sock, line + *idx, 1);
    if (rc < 0) {
      perror ("reading line");
      return (-40);
    }
    (*idx)++;
  } while (*idx < sz-1 && line[*idx-1] != LF);
  line[*idx] = 0;
  return 0;
}




/*		Send TI document back to client		send_ti_doc()
**
**
** On entry,
**	tisock		connected to server
**	nodeid		points to node valid id string
**
** On exit,
**	returns		number of bytes sent excluding header
**	document retrieved and relayed.
**
** The buffers are flushed often at first and then with decreasing frequency
** to give good pipelining performance.
*/

PRIVATE int send_ti_doc (int tisock, char *nodeid)
{
  char docbuf[20200];
#define DOC_BLKSZ (sizeof(docbuf)-200)
/* 200 extra chars for first line from TI server, contains lastmodified, etc */
  long startat;
  long totalchars;
  char *begintext, *endline;
  long totalsent = 0;
  int wrote;
  int flushlevel = 1000;	/* Flush after this times 2**k */

  startat = 0;
  do {
    sprintf (tibuf, TICMD_GETDOC, nodeid, (int)startat, (int)DOC_BLKSZ);
    sprintf (docbuf, "ToTechinfo:%s", tibuf);
    logdebug (docbuf);
    send_msg (tisock, tibuf, strlen(tibuf));
    
    /* get response to the t: command */
    get_msg (tisock, docbuf, sizeof(docbuf));

    if (startat == 0) {
      totalchars = atoi (docbuf);
    }

    /* Don't know why, but in response to t: transaction,
       the TI server sends a status line ending in LF, 
       and then sends another LF -LAM */
    for (begintext = docbuf; *begintext != LF && *begintext;
	 begintext++);
    if (*begintext == LF) begintext++;
    if (*begintext == LF) begintext++;

    *(begintext + DOC_BLKSZ) = '\0'; /* eliminating possible LF at end */
    
    fprintf(client, "<TITLE>(TechInfo document %s)</TITLE>\r\n", nodeid);
    /* @@ Not very informative! -- can get real title? */
    fprintf(client, "<PLAINTEXT>\r\n");
    
    for (; *begintext != '\0'; ) {
      for (endline=begintext; *endline && *endline != LF; endline++);
      wrote = fwrite(begintext, 1, endline-begintext, client);
      if (wrote > 0) totalsent += wrote;

      if (*endline == LF) {
	wrote = fwrite("\r\n", 1, 2, client);
	if (wrote > 0) totalsent += wrote;
	endline++;
	if (totalsent > flushlevel) {
	    fflush(client);
	    flushlevel = flushlevel + flushlevel;  /* Grow exponentially */
	}
      }
      begintext = endline; 
    }

    startat = startat + DOC_BLKSZ;
  } while (totalchars - startat > 0);

  sprintf (docbuf, "Sent %d bytes", (int)totalsent);
  logdebug (docbuf);

  if (totalsent < 1) {
    fprintf (client,
    "TechInfo gateway warning: Document seems to be nonexistent or empty.\r\n");
    fflush(client);
  }
  return totalsent;
}


PRIVATE int parse_fields (char delim, char *line, char *fields[], int maxfields)
{
  char *cp = line;
  int argcnt = 0;

  while (argcnt < maxfields) {
    fields[argcnt] = cp;
    argcnt++;
    while (*cp && *cp != delim)
      cp++;
    if (!*cp)
      break;
    *cp = 0;		/* Break fields apart */
    cp++;
  }
  fields[argcnt] = (char *) 0;
  return argcnt;
}


PRIVATE struct hostent *
gethostbyn_or_ad (host)
     char *host;
{
  u_long ip_addrl;
  struct hostent *h;

  if (isdigit (host[0])) {     /* if name begins with a digit, assume IP adr */
    ip_addrl = inet_addr (host);
    h = gethostbyaddr ((char *)&ip_addrl, (int)sizeof(u_long), AF_INET);
    return (h);
  }
  else {
    h = gethostbyname (host); 
    return (h);
  }
}


PRIVATE int getportbyn_or_num (str, port)
     int *port;
     char *str;
{
  struct servent *servent;

  if ( isdigit(str[0]) ) {   /* if its a number, convert ascii to int */
    *port = atoi(str);
    *port = htons( (u_short) *port);
  }
  else {        /* if not a number, look up name in Ultrix */
    servent = getservbyname (str, "tcp");
    if (servent == 0) {
      return (-1);
    }
    *port = servent->s_port;
  }
  return (1);
}





PRIVATE int
inet_connect(char *hostname, char *service, char **errstr)
{
#define ERRORMSG_SIZE 200
  struct hostent *host;
  int toport;
  struct sockaddr_in sin;
  int sock;
  extern char *sys_errlist[];
  extern int sys_nerr;

  *errstr = (char *) malloc (ERRORMSG_SIZE);
  (*errstr)[0] = 0;

  host = gethostbyn_or_ad (hostname);

  if (host == NULL) {
    sprintf (*errstr, "%s: Unknown host", hostname);
    return -1;
  }

  else  if (getportbyn_or_num (service, &toport) < 0)  {
    sprintf (*errstr, "%s: Unknown TCP service", service);
    return -1;
  }

  else {
    sin.sin_family = host->h_addrtype;
    bcopy(host->h_addr,  &(sin.sin_addr), host->h_length);
    sin.sin_port = toport;

    sock = socket (AF_INET, SOCK_STREAM, 0); /* Had extra 0 parameter - TBL */
    if (sock < 0) {
      strcat (*errstr, "socket: ");
     }

    else {
      if (connect (sock, (struct sockaddr*)&sin, (int)sizeof (sin)) < 0) {
	strcat (*errstr, hostname);
	strcat (*errstr, ":");
	strcat (*errstr, service);
	strcat (*errstr, ":");
	close (sock);
	sock = -1;
      }
    }
  } 

  if (errno >= 0 && errno < sys_nerr)  {
    strcat (*errstr, sys_errlist[errno]);
  }

  if (sock >= 0)
    free(*errstr);
  return (sock);
}

PRIVATE int 
send_msg(sock,str,len) /* BEWARE! this routine writes into str beyond len! */
     int sock;
     char *str;
     int len;
{
  int wrote;

  if (sock < 0) return 0;
  if (len < 1) return 0;

  str[len] = CR;
  str[len+1] = LF;
  if (TRACE) fprintf(stderr, "TIGATE: sending to socket %d: %s", sock, str);
  wrote = write(sock, str, len+2);
  if (wrote != len+2) {
    fprintf (stderr, "Wrote %d instead of %d, errno %d", wrote, len+2, errno);
    perror ("writing message");
    return (-50);
  }
  else
    return 1;
}



PRIVATE int get_msg(sock, buff, buffsize)   /* read message off socket */
     int             sock;
     char           *buff;
     int             buffsize;
{
  int rc, len;

  len = 0;
  bzero (buff, buffsize);

  do {
    rc = read(sock, &buff[len], buffsize - len);
    if (rc < 0 ) {
      perror("in read");
      break;
    }
    len = rc + len;
  } while (rc >= 0 && len <= buffsize && !terminator(&buff[len - EOM_LEN]));

  if (terminator (&buff[len - EOM_LEN]))
    len -= EOM_LEN;

  buff[len] = '\0';
  return (rc);
}




PRIVATE void logdebug (char *string)
{
#ifdef DEBUG
  FILE *debugfp;
  long now;
  struct sockaddr_in sname;
  struct hostent *host;
  char peername[100];
  int namelen;

  namelen = sizeof(sname);
  
  if (getpeername (gophinsock, (struct sockaddr*)&sname, &namelen)) 
    sprintf (peername, "getpeername-errno-%d", errno);
  else {
    host = gethostbyaddr((char*)&sname.sin_addr, sizeof(sname.sin_addr), AF_INET);
    if (!host)
      sprintf (peername, "gethostbyaddr errno %d", errno);
    else
      sprintf (peername, "%s", host->h_name);
  }

  debugfp = fopen (DEBUGLOG, "a");
  if (debugfp != NULL) {
    now = time(0);
    fprintf (debugfp, "%d:%s:%s:%s", getpid(), string, peername, ctime (&now));
    fclose(debugfp);
  }
#endif
}


/*	Make MENU item for W3 menu document		make_menu_item()
**	-----------------------------------
*/
PRIVATE void send_menu_line (char *title, char ityp,
		char *tiserver, char *tiport, char *nodeid)
{
    fprintf (client, "<LI><A HREF=\"/%s:%s/%c/%s\">%s</A>\r\n",
  	tiserver, tiport, ityp, nodeid, title);
}


/*	Relay one item from a list			send-server_line()
**	--------------------------
*/
PRIVATE  void send_server_line(char *line)
{
  char *fields[TIMAXFIELDS];
  char *nodeid, *server, *port, *title;
  char *Title;
  
  parse_fields (TIDELIM, line, fields, TIMAXFIELDS);
  nodeid = fields[0];
  port = fields[1];
  title = fields[5];
  server = fields[6];

  Title = (char *) malloc (strlen(title) +  strlen (" TechInfo") + 1);
  strcpy (Title, title);
  strcat (Title, " TechInfo");

  send_menu_line (Title, I_MENU, server, port, nodeid);

}


/*	Relay the list of all TI servers		send_ti_server_list()
**
**	The master server address is built into the gateway.
*/

PRIVATE void send_ti_server_list()
{
  char *errstr;
  int length;
  int toread, numlines;
  int tisock;


  logdebug ("Send server list");

  tisock = inet_connect (TISERVERS_HOST, TISERVERS_PORT, &errstr);
  if (tisock < 0) {
    fprintf(client,
    "Gateway Error: Couldn't get list of TechInfo servers. %s (port %s)\r\n",
	errstr, TISERVERS_PORT);
  } else {
    get_msg (tisock, tibuf, sizeof(tibuf));
    send_version(tisock);	/* waht does this do? - Tim */

    sprintf (tibuf, "%s", TICMD_GETSERVERS);
    logdebug (tibuf);

    send_msg (tisock, tibuf, strlen(tibuf));
    readline (tibuf, sizeof(tibuf), tisock, &length);

    toread = atoi (tibuf);
    numlines = 0;
    fprintf(client, "<MENU>\r\n");
    while (numlines < toread) {
      readline (tibuf, sizeof(tibuf), tisock, &length);
      numlines++;
      send_server_line(tibuf);
    }
    fprintf(client, "</MENU>\r\n");
  }
}


/*	Send one menu item				send_menu_item()
**
** On entry,
**	line			contains the TI menu line
**	tiserver		contains the server fqdn or number
**	tiport			contains the port number
**	minlevel		??
**
**
** On return,
**	returns 0		level too low,
**		1		line sent to client,
*/

PRIVATE int send_menu_item (char *line, char *tiserver, char *tiport, int minlevel)
{
  char *fields[TIMAXFIELDS];
  char *title, *filename, *nodeid, *level;

  parse_fields (TIDELIM, line, fields, TIMAXFIELDS);
  level = fields[0];
  nodeid = fields[1];
  title = fields[5];
  filename = fields[8];
  
  if (atoi(level) < minlevel)
    return 0;
  
  send_menu_line (title, strlen(filename) > 0 ? I_DOC : I_MENU,
		  tiserver, tiport, nodeid);
  return 1;
}



/*	Send the search information and/or link to server top
**
**   This tells the user what a search will do. For a non-top node,
**   it gives a link to the top.
*/
PRIVATE void send_keywordsearch_item
 (char *tiserver, char *tiport, char *nodeid, int numitems)
{
  if ((strlen(nodeid) > 0) && strcmp(nodeid, "0")!=0) {
    fprintf(client,
    	"%s\r\nSee also the <A HREF=\"/%s:%s/M/0\">whole server</A>.\r\n",
    	"Supply keywords to search the information under this menu.",
	tiserver, tiport);
  } else {
    fprintf (client, 
      "%s\r\n(This server is at %s on port %s.)\r\n",
      "Supply keywords to search all information at this TechInfo server",
	     tiserver, tiport);
  }


}




PRIVATE void send_ti_nodelist (int tisock, char *tiserver, char *tiport, int *sent, int minlevel)
{
  int toread, numlines;
  int length;

  readline (tibuf, sizeof(tibuf), tisock, &length);
  toread = atoi (tibuf);
  numlines = 0;
  *sent = 0;
  while (numlines < toread) {
    readline (tibuf, sizeof(tibuf), tisock, &length);
    /*	logdebug (tibuf); */
    numlines++;
    *sent += send_menu_item (tibuf, tiserver, tiport, minlevel);
    if ((numlines % FLUSHLINES) == 0) fflush(client);
  }
  sprintf (tibuf, "Sent %d nodes", *sent);
  logdebug (tibuf);
  readline (tibuf, sizeof(tibuf), tisock, &length);
}

/*				Special Cases
*/

/*		Welcome Page
**
*/
PRIVATE int welcome()
{
    fprintf(client, "<TITLE>Welcome to the TechInfo Gateway</TITLE>\r\n");
    fprintf(client, "<H1>TechInfo Gateway</H1>\r\n");
    fprintf(client,
    "Welcome to the WorldWideWeb-TechInfo gateway.<p>\r\n");
    fprintf(client,
    "Information available through this service is that\r\n");
    fprintf(client,
    "served by servers running the TechInfo protocol from MIT.\r\n");
    fprintf(client,
    "<P>Servers running this protocol currently include the following:\r\n");
    fprintf(client, "\r\n");
    fflush(client);

    send_ti_server_list();

    fflush(client);
    return 1;
}


/* The page below may be best just written as a hypertext file - Tim
*/

PRIVATE int terminalbased_ti()
{
  char * fmt = "<LI><A HREF=\"telnet://%s/\">%s</A>\r\n";
  fprintf(client,
  "<title>Terminal-based Techinfo Servers</title>\r\n");
  fprintf(client,
  "<H1>Terminal-based Techinfo Servers</H1>\r\n");
  fprintf(client, "<MENU>\r\n");
  fprintf(client, fmt,
        "techinfo.mit.edu", "Massachusetts Institute of Technology");
  fprintf(client, fmt,
  	"msuinfo.msstate.edu", "Mississippi State");
  fprintf(client, fmt,
  	"penninfo.upenn.edu", "University of Pennsylvania");
  fprintf(client, fmt,
  	"penninfo.upenn.edu:9005", "University of Pennsylvania TEST");
  fprintf(client, "</MENU>\r\n");
  fflush(client);
  return 1;
}

PRIVATE int do_about_gw()
{
  FILE *fp;
  char line[100], *cp;

  fp = fopen (MSGFILE, "r");
  if (fp == NULL) {
    fprintf (client, "Unable to read msg file %s\r\n", MSGFILE);
    fflush(client);
    exit (-2);
  }
  do {
    if (fgets (line,sizeof(line)-1,fp) == NULL)
      break;
    else {
      cp = index (line, '\n');
      if (cp) {
	fwrite (line, 1, cp-line, client);
	fprintf(client, "\r\n");
      }
      else
	fprintf(client, line);
    }
  } while (1);
  return 1;
}


/*		Retrieve a document
**
**  This routine is called either by the test program at the bottom of
**  this file or by the HTDaemon code.
**
** On entry,
**	soc		A file descriptor for input and output.
** On exit,
**	returns		>0	Channel is still open.
**			0	End of file was found, please close file
**			<0	Error found, please close file.
*/

PUBLIC int HTRetrieve(char *request, char *keywords, int soc)
{
    char *fields[5];		/* allow one extra for zero term! TBL*/
    int tisock;
    char *tiserver, *tiport, *typ, *nodeid;
    char *errstr;
    int numitems;
    
    client = fdopen(soc, "w");		/* open normal C file pointer */
    if (!client) return -1;

    if (request[1] == 0)                 /* is it a single / ? */
	return welcome();
	
    if (!strcmp(request, ABOUT_GW))
    	return do_about_gw();

    if (!strcmp(request, TERMBASEDTI))
    	return terminalbased_ti();

             /* should be a techinfo menu request */

    if (parse_fields ('/', request, fields, 4) < 3) {
	fprintf(client,
	"Gateway error: Bad document ID\r\n");
    	fflush(client);
	return 1;
    }
    
    tiserver = fields[1];
    typ = fields[2];
    nodeid = fields[3];
    if (!nodeid) nodeid="0";
    
    parse_fields(':', tiserver, fields, 2);
    tiport = fields[1];
    if (!tiport) tiport = "9000";	/* Default port? @@ */


/* okay, we've parsed the Intermed. token... */

    tisock = inet_connect (tiserver, tiport, &errstr);
    if (tisock < 0) {
	fprintf(client,
	"Gateway error: Couldn't connect to Techinfo:%s (port %s)\r\n",
		errstr, tiport);
    	fflush(client);
	return 1;
    }
    
    get_msg (tisock, tibuf, sizeof(tibuf));
    send_version(tisock);

    if (keywords) {				/* Search */
    
        fprintf(client, "Information with keywords `%s' in this index\r\n",
		keywords);
	fflush(client);	/* Raise expectations by printing something */
	
	if ((strlen(nodeid) > 0) && (strcmp(nodeid, "0")!=0))
	      sprintf (tibuf, TICMD_KEYSRCH_NODEID, keywords, nodeid);
	else
	      sprintf (tibuf, TICMD_KEYSRCH, keywords);

	send_msg (tisock, tibuf, strlen(tibuf));

	if (numitems == 0) {
	    fprintf(client,
	        "Sorry, no information with those keywords.<p>\r\n");
	}
	
	fprintf(client, "<ISINDEX>\r\n<MENU>\r\n");
	send_ti_nodelist(tisock, tiserver, tiport, &numitems, 0);
	fprintf(client, "</MENU>\r\n");

    } else if (*typ == I_DOC) {			/* Relay document */
	send_ti_doc (tisock, nodeid);
	
    } else { 					/* Relay menu */

	sprintf (tibuf, TICMD_GETMENU, nodeid);
	send_msg (tisock, tibuf, strlen(tibuf));

	fprintf(client, "<ISINDEX>\r\n<MENU>\r\n");
	send_ti_nodelist (tisock, tiserver, tiport, &numitems, 1);
	fprintf(client, "</MENU>\r\n");
	send_keywordsearch_item (tiserver, tiport, nodeid, numitems);
    }

    fflush(client);
    return 1;
}

PRIVATE void send_version(int tisock)
{
  sprintf (tibuf, TICMD_VERSION, GWVERSION);
  send_msg (tisock, tibuf, strlen(tibuf));
  get_msg(tisock, clientbuf, sizeof(clientbuf));
}

#ifdef TEST		/* Otherwise, use HTDaemon.c */

/*		Get request line				   gerreq()
*/

PRIVATE void getreq (char *request, int max, int insock)
{
  int idx;
  char *cp;

  readline (request, max, insock, &idx);
  cp = index (request, CR);
  if (cp) *cp = 0;
  cp = index (request, LF);
  if (cp) *cp = 0;
}


/*		M A I N     P R O G R A M
**		=======	    =============
**
**   See also the HTDaemon.c file fro a more complete daemon shell.
*/
void main (int argc, char *argv[])
{
    char request[1000];
    char *ptr;
    char * keywords;
    
    setreuid (RUNTIME_UID, RUNTIME_UID);
    if (getuid() == 0 || geteuid() == 0) {
    	fprintf (stderr, 
	  "THIS SERVER NOT PERMITTED TO RUN AS PRIVILEGED UID\n");
    	fprintf(client, 
	  "THIS SERVER NOT PERMITTED TO RUN AS PRIVILEGED UID\r\n");
    	fflush(stderr);
    	fflush(client);
    exit (-1);
    }
    
    client = stdout;
    gophinsock = fileno(stdin);
    
    
    logdebug ("Start");

/*	(Note: needed to know own address as Gopher server, not needed
**	for WWW server. - TBL )
*/

    getreq (request, sizeof(request), gophinsock);
    
    for (ptr = request; *ptr && isspace(*ptr) ; ptr++);
    
    if (strncmp(ptr, "GET ", 4)!=0) {
	fprintf(client, "Unrecognised request format %s\r\n", ptr);
	return;
    }
  
    ptr = ptr +4;
  
    keywords = strchr(request, '?');
    if (keywords) {
	*keywords++ = 0;		/* Chop keywords off */
	if (!*keywords) keywords = NULL;
	else {
	    char *p = keywords;
	    while ((p = strchr(p, '+')) != 0) *p = ' '; /* Plusses to spaces */
	}
    }
    
    HTRetrieve(ptr, keywords, 1);
    
    logdebug ("End");
    exit (0);
}
#endif /* TEST */
