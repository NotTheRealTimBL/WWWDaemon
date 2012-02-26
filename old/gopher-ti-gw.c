/* 		WWW  --> TechInfo Gateway
  
  
  Copyright November 1992 by the University of Pennsylvania.

  

Authors:
	LAM	Linda Murphy, University of Pennsylvania
	TBL	Tim Berners-Lee, CERN

History:
	   Dec 92	WWW gateway started (TBL) 
	   Nov 92	Written as Gopher Gateway (LAM)

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
                                        Univ of Minn Gopher

GET /tihost:tiport/type/tinodeid         Results of Techinfo transaction

GET /tihost:tiport/type/tinodeid?word+word
                                         Results of Techinfo search

GET /doc/worldwide-techinfo              List of TechInfo servers

GET /doc/WWW-TI-Gateway                  Document -- about this gopher

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
   
GET /ti-srv.upenn.edu:9000/K/120?Internet+Gopher

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

/* CODE STARTS HERE --- not yet modified.  */

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


#define LOCALTI_SERVER      "penninfo-srv.upenn.edu"
#define LOCALTI_PORT          "9000"
#define LOCALTI_MAINMENU   "0"
#define DEBUGLOG "/usr/users/murphy/goph_ti_gw.log" 
#define MSGFILE  "/usr/users/murphy/goph_ti_gw.msg"
#define GWVERSION "GOPH-TIGW:1.1"

/* Which host is the keeper of the TechInfo servers worldwide? */

#ifndef TISERVERS_HOST
#define TISERVERS_HOST  "tiserve.mit.edu"
#endif

#ifndef TISERVERS_PORT
#define TISERVERS_PORT  "9000"
#endif

#define ABOUT_GW      "Goph-TI-Gateway"   /* About this gopher */
#define TERMBASEDTI   "telnet-techinfo"     /* Telnet session to techinfo */
#define WORLDWIDETI   "worldwide-techinfo"  /* List of all TechInfo servers */

int gophsock = -1;
int gophinsock = -1;

char tibuf[200000];
char gophbuf[10000];
char *gatewayhost;
char *gatewayport;

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

/* Gopher Protocol */
#define GO_DOC		'0'
#define GO_MENU		'1'
#define GO_SEARCH       '7'
#define GO_TELNET       '8'
#define GO_ERR		'-'
#define GO_DELIM        '\t'

/* Intermediary protocol */
#define I_DELIM         ' '
#define I_DOC           'D'
#define I_MENU          'M'
#define I_KEYSRCH       'K'
#define I_KEYSRCH_END   ':'



getreq (char *request, int max, int insock)
{
  int idx;
  char *cp;

  readline (request, max, insock, &idx);
  cp = index (request, CR);
  if (cp) *cp = 0;
  cp = index (request, LF);
  if (cp) *cp = 0;
}




int send_ti_doc (int tisock, int gophsock, char *nodeid)
{
  char docbuf[20200];
#define DOC_BLKSZ (sizeof(docbuf)-200)
/* 200 extra chars for first line from TI server, contains lastmodified, etc */
  long startat;
  long totalchars;
  char *begintext, *endline;
  long totalsent = 0;
  int wrote;

  startat = 0;
  do {
    sprintf (tibuf, TICMD_GETDOC, nodeid, startat, DOC_BLKSZ);
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
       and then sends another LF */
    for (begintext = docbuf; *begintext != LF && *begintext;
	 begintext++);
    if (*begintext == LF) begintext++;
    if (*begintext == LF) begintext++;

    *(begintext + DOC_BLKSZ) = '\0'; /* eliminating possible LF at end */
    
    for (; *begintext != '\0'; ) {
      for (endline=begintext; *endline && *endline != LF; endline++);
      wrote = write (gophsock, begintext, endline-begintext);
      if (wrote > 0) totalsent += wrote;

      if (*endline == LF) {
	wrote = write (gophsock, "\r\n", 2);
	if (wrote > 0) totalsent += wrote;
	endline++;
      }
      begintext = endline; 
    }

    startat = startat + DOC_BLKSZ;
  } while (totalchars - startat > 0);

  sprintf (docbuf, "Sent %d bytes", totalsent);
  logdebug (docbuf);

  if (totalsent < 1) {
    sprintf (docbuf, "%c%s", GO_ERR, " File does not exist!!!!!");
    send_msg (gophsock, docbuf, strlen(docbuf));
  }
  return totalsent;
}


int parse_fields (char delim, char *line, char *fields[], int maxfields)
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
    *cp = 0;
    cp++;
  }
  fields[argcnt] = (char *) 0;
  return argcnt;
}


struct hostent *
gethostbyn_or_ad (host)
     char *host;
{
  u_long ip_addrl;
  struct hostent *h;

  if (isdigit (host[0])) {     /* if name begins with a digit, assume IP adr */
    ip_addrl = inet_addr (host);
    h = gethostbyaddr (&ip_addrl, sizeof(u_long), AF_INET);
    return (h);
  }
  else {
    h = gethostbyname (host); 
    return (h);
  }
}


int getportbyn_or_num (str, port)
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





int
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

    sock = socket (AF_INET, SOCK_STREAM, 0, 0);
    if (sock < 0) {
      strcat (*errstr, "socket: ");
     }

    else {
      if (connect (sock, &sin, sizeof (sin)) < 0) {
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

int 
send_msg(sock,str,len) /* BEWARE! this routine writes into str beyond len! */
     int sock;
     char *str;
     int len;
{
  int wrote;
  char msg[100];

  if (sock < 0) return 0;
  if (len < 1) return 0;

  str[len] = CR;
  str[len+1] = LF;
  wrote = write(sock, str, len+2);
  if (wrote != len+2) {
    fprintf (stderr, "Wrote %d instead of %d, errno %d", wrote, len+2, errno);
    perror ("writing message");
    return (-50);
  }
  else
    return 1;
}


int readline (char *line, int sz, int sock, int *idx)
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
}


get_msg(sock, buff, buffsize)   /* read message off socket */
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




logdebug (char *string)
{
#ifdef DEBUG
  FILE *debugfp;
  long now;
  struct sockaddr_in sname;
  struct hostent *host;
  char peername[100];
  int namelen;

  namelen = sizeof(sname);
  
  if (getpeername (gophinsock, &sname, &namelen)) 
    sprintf (peername, "getpeername-errno-%d", errno);
  else {
    host = gethostbyaddr(&sname.sin_addr, sizeof(sname.sin_addr), AF_INET);
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

make_menu_line (char *line, char typ, char *title, char ityp,
		char *tiserver, char *tiport, char *nodeid)
{
  sprintf (line, "%c%s%c%c%c%s%c%s%c%s%c%s%c%s",
	   typ, title, GO_DELIM,
	   ityp, I_DELIM, tiserver, I_DELIM, tiport, I_DELIM, nodeid, GO_DELIM,
	   gatewayhost, GO_DELIM, gatewayport);
}

static void send_server_line(char *line)
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

  make_menu_line (gophbuf, GO_MENU, Title, I_MENU, server, port, nodeid);
/*  logdebug (gophbuf); */
  send_msg (gophsock, gophbuf, strlen(gophbuf));
}

send_ti_server_list()
{
  char *errstr;
  int length;
  int toread, numlines;
  int tisock;


  logdebug ("Send server list");

  tisock = inet_connect (TISERVERS_HOST, TISERVERS_PORT, &errstr);
  if (tisock < 0) {
    sprintf(gophbuf, "%cCouldn't get list of TechInfo servers. %s (port %s)",
	    GO_DOC, errstr, TISERVERS_PORT);

    logdebug(gophbuf);
    send_msg (gophsock, gophbuf, strlen(gophbuf));
  }
  else {
    get_msg (tisock, tibuf, sizeof(tibuf));
    send_version(tisock);

    sprintf (tibuf, "%s", TICMD_GETSERVERS);
    sprintf (gophbuf, "ToTechinfo:%s", tibuf);
    logdebug (gophbuf);

    send_msg (tisock, tibuf, strlen(tibuf));
    readline (tibuf, sizeof(tibuf), tisock, &length);

    toread = atoi (tibuf);
    numlines = 0;
  
    while (numlines < toread) {
      readline (tibuf, sizeof(tibuf), tisock, &length);
      numlines++;
      send_server_line(tibuf);
    }
  }
}


static int send_menu_item (char *line, char *tiserver, char *tiport, int minlevel)
{
  char *fields[TIMAXFIELDS];
  char selector[500];
  char *title, *filename, *nodeid, *level;

  parse_fields (TIDELIM, line, fields, TIMAXFIELDS);
  level = fields[0];
  nodeid = fields[1];
  title = fields[5];
  filename = fields[8];
  
  if (atoi(level) < minlevel)
    return 0;
  
  make_menu_line (gophbuf, strlen(filename) > 0 ? GO_DOC : GO_MENU,
		  title, strlen(filename) > 0 ? I_DOC : I_MENU,
		  tiserver, tiport, nodeid);
/*  logdebug(gophbuf);  */
  send_msg (gophsock, gophbuf, strlen(gophbuf));
  return 1;
}

send_keywordsearch_item (char *tiserver, char *tiport, char *nodeid, int numitems)
{
  char title [500];

  if (strlen (nodeid) > 0) 
    sprintf (title, "Keyword search the folders in this menu", numitems);
  else
    sprintf (title, "Keyword search all nodes at this TechInfo server",
	     tiserver);

  sprintf (gophbuf, "%c%s%c%c%c%s%c%s%c%s%c%s%c%s",
	   GO_SEARCH, title, GO_DELIM,
	   I_KEYSRCH, I_DELIM, tiserver,
	   I_DELIM, tiport, I_DELIM, nodeid,
	   GO_DELIM, gatewayhost, GO_DELIM, gatewayport);
  send_msg (gophsock, gophbuf, strlen(gophbuf));
}




send_ti_nodelist (int tisock, char *tiserver, char *tiport, int *sent, int minlevel)
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
  }
  sprintf (tibuf, "Sent %d nodes", *sent);
  logdebug (tibuf);
  readline (tibuf, sizeof(tibuf), tisock, &length);
}




static void do_ti_trans(char *request)
{
  char *fields[4];
  int tisock;
  char *tiserver, *tiport, *typ, *nodeid;
  char *errstr;
  char *keywordtarget = NULL;
  int numitems;
  
  switch (*request) {
  case I_KEYSRCH:
    /* expect selector-string TAB search-string */
    keywordtarget = index (request, GO_DELIM);
    if (!keywordtarget) {
      sprintf (gophbuf, "%cDidn't understand request %s", GO_ERR,request);
      logdebug(gophbuf);
      send_msg (gophsock, gophbuf, strlen(gophbuf));
      return;
    }
    /* Replace the delimiter with end-of-string marker */
    *keywordtarget = '\0';
    keywordtarget++;
    /* fall through to parse_fields intentionally */

  case I_DOC:
  case I_MENU:
    
    parse_fields (I_DELIM, request, fields, 4);
    typ = fields[0];
    tiserver = fields[1];
    tiport = fields[2];
    nodeid = fields[3];
    break;
    
  default:
    sprintf (gophbuf, "Didn't understand file type %c", *request);
    logdebug(gophbuf);

    sprintf (gophbuf, ".");
    send_msg (gophsock, gophbuf, strlen(gophbuf));
    return;
  }

/* okay, we've parsed the Intermed. token... */


/*  printf ("serv=%s, port=%s, nodeid=%s, target=%s.\n",
	  tiserver, tiport, nodeid, keywordtarget); */

  tisock = inet_connect (tiserver, tiport, &errstr);
  if (tisock < 0) {
    sprintf(gophbuf, "%cCouldn't connect to Techinfo:%s (port %s)",
	    GO_ERR, errstr, tiport);
    logdebug(gophbuf);
  }
  else {
    get_msg (tisock, tibuf, sizeof(tibuf));
    send_version(tisock);

    switch (*typ) {
    case I_KEYSRCH:
      if (strlen(nodeid) > 0)
	sprintf (tibuf, TICMD_KEYSRCH_NODEID, keywordtarget, nodeid);
      else
	sprintf (tibuf, TICMD_KEYSRCH, keywordtarget);

      sprintf (gophbuf, "ToTechinfo:%s", tibuf);
      logdebug (gophbuf);

      send_msg (tisock, tibuf, strlen(tibuf));
      send_ti_nodelist(tisock, tiserver, tiport, &numitems, 0);
      break;

    case I_DOC:
      send_ti_doc (tisock, gophsock, nodeid);
      break;

    case I_MENU:
      sprintf (tibuf, TICMD_GETMENU, nodeid);
      sprintf (gophbuf, "ToTechinfo:%s", tibuf);
      logdebug (gophbuf);
      send_msg (tisock, tibuf, strlen(tibuf));

      send_ti_nodelist (tisock, tiserver, tiport, &numitems, 1);
      send_keywordsearch_item (tiserver, tiport, "", 0);
      send_keywordsearch_item (tiserver, tiport, nodeid, numitems);

      break;

    }
  }
}

terminalbased_ti()
{
  sprintf (gophbuf, "%cMassachusetts Institute of Technology%c%ctechinfo.mit.edu%c0",
	   GO_TELNET, GO_DELIM, GO_DELIM,GO_DELIM);
  send_msg (gophsock, gophbuf, strlen(gophbuf));

  sprintf (gophbuf, "%cMississippi State%cMSUINFO%cmsuinfo.msstate.edu%c0",
	   GO_TELNET, GO_DELIM, GO_DELIM,GO_DELIM);
  send_msg (gophsock, gophbuf, strlen(gophbuf));

  sprintf (gophbuf, "%cUniversity of Pennsylvania%c%cpenninfo.upenn.edu%c0",
	   GO_TELNET, GO_DELIM, GO_DELIM,GO_DELIM);
  send_msg (gophsock, gophbuf, strlen(gophbuf));

  sprintf (gophbuf, "%cUniversity of Pennsylvania TEST%c%cpenninfo.upenn.edu%c9005",
	   GO_TELNET, GO_DELIM, GO_DELIM,GO_DELIM);
  send_msg (gophsock, gophbuf, strlen(gophbuf));
}

do_about_gw()
{
  FILE *fp;
  char line[100], *cp;

  fp = fopen (MSGFILE, "r");
  if (fp == NULL) {
    sprintf (gophbuf, "Unable to read msg file %s", MSGFILE);
    logdebug (gophbuf);
    logdebug ("End");
    exit (-2);
  }
  do {
    if (fgets (line,sizeof(line)-1,fp) == NULL)
      break;
    else {
      cp = index (line, '\n');
      if (cp) {
	write (gophsock, line, cp-line);
	write (gophsock, "\r\n", 2);
      }
      else
	write (gophsock, line, strlen(line));
    }
  } while (1);
}

send_version(int tisock)
{
  sprintf (tibuf, TICMD_VERSION, GWVERSION);
  send_msg (tisock, tibuf, strlen(tibuf));
  get_msg(tisock, gophbuf, sizeof(gophbuf));
}

main (int argc, char *argv[])
{
  char request[1000];
  char *ptr;

  setreuid (RUNTIME_UID, RUNTIME_UID);
  if (getuid() == 0 || geteuid() == 0) {
    fprintf (stdout, "%cTHIS SERVER NOT PERMITTED TO RUN AS PRIVILEGED UID\n",GO_ERR);
    fflush(stdout);
    exit (-1);
  }

  gophsock =  fileno(stdout);
  gophinsock = fileno(stdin);


  logdebug ("Start");

  if (argc < 3) {
    fprintf (stderr, "Usage: %s hostname port\r\n", argv[0]);
    exit (-1);
  }
  gatewayhost = argv[1];
  gatewayport = argv[2];

  getreq (request, sizeof(request), gophinsock);
	sprintf (gophbuf, "gopher:%s", request);
  logdebug (gophbuf);

  for (ptr = request; *ptr && isspace(*ptr) ; ptr++);
  if (*ptr == 0) {                 /* is it a blank line? */
    sprintf (gophbuf, "%cAbout this Internet Gopher%c%s%c%s%c%s",
	    GO_DOC, GO_DELIM, ABOUT_GW, GO_DELIM,
	     gatewayhost, GO_DELIM, gatewayport);
    send_msg (gophsock, gophbuf, strlen(gophbuf));

    sprintf (request, "%c%c%s%c%s%c%s",
       I_MENU, I_DELIM,  LOCALTI_SERVER, I_DELIM, LOCALTI_PORT, I_DELIM, 
        LOCALTI_MAINMENU);
    do_ti_trans(request);

    sprintf (gophbuf, "%cWorld Wide TechInfo%c%s%c%s%c%s",
       GO_MENU, GO_DELIM, WORLDWIDETI, GO_DELIM,
        gatewayhost, GO_DELIM, gatewayport);
    send_msg (gophsock, gophbuf, strlen(gophbuf));

    sprintf (gophbuf, "%cWorld Wide Gopher%c1/Other Gopher and Information Servers%cgopher.tc.umn.edu%c70",
	     GO_MENU, GO_DELIM, GO_DELIM,GO_DELIM);
    send_msg (gophsock, gophbuf, strlen(gophbuf));
  }

	else if (!strcmp(request, WORLDWIDETI)) {
     send_ti_server_list();
     sprintf (gophbuf, "%cTerminal-based TechInfo services%c%s%c%s%c%s",
	     GO_MENU, GO_DELIM, TERMBASEDTI, GO_DELIM,
	     gatewayhost, GO_DELIM, gatewayport);
     send_msg (gophsock, gophbuf, strlen(gophbuf));
}

  else if (!strcmp(request, ABOUT_GW))
    do_about_gw();

  else if (!strcmp(request, TERMBASEDTI))
    terminalbased_ti();

  else                            /* should be a techinfo menu request */
    do_ti_trans(request);

  strcpy (gophbuf, ".");
  send_msg (gophsock, gophbuf, strlen(gophbuf));

  close (gophsock);
  logdebug ("End");

  fflush(stdout);
  exit (0);
}
