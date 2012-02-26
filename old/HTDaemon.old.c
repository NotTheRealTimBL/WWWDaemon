/*		TCP/IP based server for HyperText		daemon.c
**		---------------------------------
**
**  History:
**	   Sep 91	Made from earlier daemon files. TBL
**	26 Feb 92	Bug fixes for Multinet. JFG
*/
/* (c) CERN WorldWideWeb project 1990,91. See Copyright.html for details */

#define VERSION 	"v0.2"

/*	Module parameters:
**	-----------------
**
**  These may be undefined and redefined by syspec.h
*/

#define LISTEN_BACKLOG 2	/* Number of pending connect requests (TCP)*/
#define MAX_CHANNELS 20		/* Number of channels we will keep open */
#define WILDCARD '*'	    	/* Wildcard used in addressing */
#define FIRST_TCP_PORT	5000	/* When using dynamic allocation */
#define LAST_TCP_PORT 5999

#define RULE_FILE		"/etc/httpd.conf"

#include "HTUtils.h"
#include "tcp.h"		/* The whole mess of include files */
#include "HTTCP.h"		/* Some utilities for TCP */

#ifdef RULES			/* Use rules? */
#include "HTRules.h"
#endif

#ifdef __STDC__
extern int HTRetrieve(char * arg, char * keywords, int soc);	/* Handle one request */
#else
extern int HTRetrieve();	/* Handle one request */
#endif

PUBLIC char HTClientHost[16];	/* Peer internet address */

/*	Module-Global Variables
**	-----------------------
*/
PRIVATE enum role_enum {master, slave, transient, passive} role;
PRIVATE struct sockaddr_in  soc_address;  /* See netinet/in.h 			*/
PRIVATE int		    soc_addrlen;
PRIVATE int		    master_soc;	  /* inet socket number to listen on	*/
PRIVATE int		    com_soc;	  /* inet socket number to read on	*/
PRIVATE fd_set		    open_sockets; /* Mask of channels which are active */
PRIVATE int		    num_sockets;  /* Number of sockets to scan */
PRIVATE BOOLEAN		    dynamic_allocation;	/* Search for port? */


/*	Program-Global Variables
**	------------------------
*/

PUBLIC int    WWW_TraceFlag;	/* Control flag for diagnostics */
PUBLIC FILE * logfile = 0;	/* Log file if any */
PUBLIC char * log_file_name = 0;/* Log file name if any (WAIS code) */


/*	Strip white space off a string
**
** On exit,
**	Return value points to first non-white character, or to 0 if none.
**	All trailing white space is overwritten with zero.
*/
PRIVATE char * strip ARGS1(char *, s)
{
#define SPACE(c) ((c==' ')||(c=='\t')||(c=='\n')||(c=='\r')) 	/*  DMX */
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
PUBLIC int HTWriteASCII ARGS2(int, soc, char *, s)
{
#ifdef NOT_ASCII
    char * p;
    char ascii[255];
    char *q = ascii;
    for (p=s; *p; p++) {
        *q++ = TOASCII(*p);
    }
    return NETWRITE(soc, ascii, p-s);
#else
    return NETWRITE(soc, s, strlen(s));
#endif
}



/*		Bind to a TCP port
**		------------------
**
** On entry,
**	tsap	is a string explaining where to take data from.
**		"" 	means data is taken from stdin.
**		"*:1729" means "listen to anyone on port 1729"
**
** On exit,
**	returns		Negative value if error.
*/
int do_bind ARGS1(CONST char *, tsap)
{

    FD_ZERO(&open_sockets);	/* Clear our record of open sockets */
    num_sockets = 0;
    
/*  Deal with PASSIVE socket:
**
**	A passive TSAP is one which has been created by the inet daemon.
**	It is indicated by a void TSAP name.  In this case, the inet
**	daemon has started this process and given it, as stdin, the connection
**	which it is to use.
*/
    if (*tsap == 0) {			/* void tsap => passive */

	dynamic_allocation = FALSE;		/* not dynamically allocated */
	role = passive;	/* Passive: started by daemon */

#ifdef vms

	{   unsigned short channel;	    /* VMS I/O channel */
	    struct string_descriptor {	    /* This is NOT a proper descriptor*/
		    int size;		    /*  but it will work.	      */
		    char *ptr;		    /* Should be word,byte,byte,long  */
	    } sys_input = {10, "SYS$INPUT:"};
	    int	status;		    /* Returned status of assign */
	    extern int sys$assign();

	    status = sys$assign(&sys_input, &channel, 0, 0);
	    com_soc = channel;	/* The channel is stdin */
	    CTRACE(tfp, "IP: Opened PASSIVE socket %d\n", channel);
	    return 1 - (status&1);
	}	
#else
	com_soc = 0;	    /* The channel is stdin */
	CTRACE(tfp, "IP: PASSIVE socket 0 assumed from inet daemon\n");
	return 0;		/* Good */
#endif

/*  Parse the name (if not PASSIVE)
*/
    } else {				/* Non-void TSAP */
	char *p;		/* pointer to string */
	char *q;
	struct hostent  *phost;	    /* Pointer to host - See netdb.h */
	char buffer[256];		/* One we can play with */
	register struct sockaddr_in* sin = &soc_address;

	strcpy(buffer, tsap);
	p = buffer;

/*  Set up defaults:
*/
	sin->sin_family = AF_INET;	    /* Family = internet, host order  */
	sin->sin_port = 0;		    /* Default: new port,    */
	dynamic_allocation = TRUE;	    /*  dynamically allocated */
	role = passive; 		    /*  by default */

/*  Check for special characters:
*/
	if (*p == WILDCARD) {		/* Any node */
	    role = master;
	    p++;
	}

/*  Strip off trailing port number if any:
*/
	for(q=p; *q; q++)
	    if (*q==':') {
	        int status;
		*q++ = 0;		/* Terminate node string */
		sin->sin_port = htons((unsigned short)HTCardinal(
					    &status, &q, (unsigned int)65535));
		if (status<0) return status;
		
		if (*q) return -2;  /* Junk follows port number */
		dynamic_allocation = FALSE;
		break;	    /* Exit for loop before we skip the zero */
	    } /*if*/

/* Get node name:
*/
	if (*p == 0) {
	    sin->sin_addr.s_addr = INADDR_ANY; /* Default: any address */

	} else if (*p>='0' && *p<='9') {   /* Numeric node address: */
	    sin->sin_addr.s_addr = inet_addr(p); /* See arpa/inet.h */

	} else {		    /* Alphanumeric node name: */
	    phost=gethostbyname(p);	/* See netdb.h */
	    if (!phost) {
		CTRACE(tfp, "IP: Can't find internet node name `%s'.\n",p);
		return HTInetStatus("gethostbyname");  /* Fail? */
	    }
	    memcpy(&sin->sin_addr, phost->h_addr, phost->h_length);
	}

	CTRACE(tfp, 
	    "Daemon: Parsed address as port %d, inet %d.%d.%d.%d\n",
		    (unsigned int)ntohs(sin->sin_port),
		    (int)*((unsigned char *)(&sin->sin_addr)+0),
		    (int)*((unsigned char *)(&sin->sin_addr)+1),
		    (int)*((unsigned char *)(&sin->sin_addr)+2),
		    (int)*((unsigned char *)(&sin->sin_addr)+3));

    } /* scope of p */


/*  Master socket for server:
*/
    if (role == master) {

/*  Create internet socket
*/
	master_soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	  
	if (master_soc<0)
	    return HTInetStatus("socket");
      
	CTRACE(tfp, "IP: Opened socket number %d\n", master_soc);
	
/*  If the port number was not specified, then we search for a free one.
*/
	if (dynamic_allocation) {
	    unsigned short try;
	    for (try=FIRST_TCP_PORT; try<=LAST_TCP_PORT; try++) { 
		soc_address.sin_port = htons(try);
		if (bind(master_soc,
			(struct sockaddr*)&soc_address,
				/* Cast to generic sockaddr */
			sizeof(soc_address)) == 0)
		    break;
		if (try == LAST_TCP_PORT)
		    return HTInetStatus("bind");
	    }
	    CTRACE(tfp, "IP:  Bound to port %u.\n",
		    ntohs(soc_address.sin_port));
	} else {					/* Port was specified */
	    if (bind(master_soc,
		     (struct sockaddr*)&soc_address,	/* Case to generic address */
		     sizeof(soc_address))<0)
		return HTInetStatus("bind");
	}
	if (listen(master_soc, LISTEN_BACKLOG)<0)
	    return HTInetStatus("listen");

	CTRACE(tfp, "Daemon: Master socket(), bind() and listen() all OK\n");
	FD_SET(master_soc, &open_sockets);
	if ((master_soc+1) > num_sockets) num_sockets=master_soc+1;

	return master_soc;
    } /* if master */
    
    return -1;		/* unimplemented role */

} /* do_bind */



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
PUBLIC int HTHandle ARGS1(int, soc)
{
#define COMMAND_SIZE	2048            /* @@@@ WAIS queries can be big! */
#define MAX_LINES 10000			/* Maximum number of lines returned */
    char command[COMMAND_SIZE+1];
    char *keywords;	/* pointer to keywords in address */
    int	status;
    char * arg;		/* Pointer to the argument string */
    
    time_t theTime;	/* A long integer giving the datetime in secs */
       
    for (;;) {
        int allocated = COMMAND_SIZE;
	command = (char *)malloc(allocated+1);
	if (!command) {
	    fprintf(stderr, "Daemon: insufficient memrory!\n");
	    exit(-5);
	}
        for(;;) {
	    if (allocated - bytes_read < MINIMUM_READ) {
	        allocated = allocated + ALLOCATION_UNIT;
		command = (char *)realloc(command, allocated+1);
		if (!command) {
		  fprintf(stderr, "Daemon: No memory to reallocate command!!\n");
		  exit(-6);
		}
	    }
    	    status = NETREAD(soc, command + bytes_read, allocated - bytes_read);
	    if (TRACE) fprintf(stderr, "NETREAD returned %d, errono=%d\n", status, errno);
	    if (status<=0) {
	        free(command);
	        return status;	/* Return and close file */
	    }
	    {
	      char * p= command + bytes_read;
	      for(p=command + bytes_read; p < command + bytes_read + status; p++) {
		if (!*p) {
		  free(command);
		  return -1;    /* No LF */
		}
	        if (*p==10) {
		  break;
		}
	      }
	      bytes_read = bytes_read + status;
	      if (p=command+bytes_read)/* No terminators yet */
		continue;
	    }
	    break;/* Terminator found */
	}
	command[bytes_read]=0;	/* terminate the string */
#ifdef NOT_ASCII
	{
	    char * p;
	    for (p=command; *p; p++) {
		*p = FROMASCII(*p);
		if (*p == '\n') *p = ' ';
	    }
	}
#endif

/*	Log the call:
*/
	if (logfile) {
	    time(&theTime);
	    fprintf(logfile, "%24.24s %s %s",
		ctime(&theTime), HTClientHost, command);
	    fflush(logfile);	/* Actually update it on disk */
	    if (TRACE) printf("Log: %24.24s %s %s",
		ctime(&theTime), HTClientHost, command);
	}
	
	arg=strchr(command,' ');
	if (arg) {
	    int stat;
	    *arg++ = 0;				/* Terminate command 	*/
	    arg = strip(arg);			/* Strip space 		*/

	    if (0==strcmp("GET", command)) {	/* Get a file 		*/
		keywords=strchr(arg, '?');
		if (keywords) *keywords++=0;	/* Split into two */
	        stat =  HTRetrieve(arg, keywords, soc);
		free(command);
		return stat;
	    }

	} /* Space found supplied */

	HTWriteASCII(soc, "<h1>HTTP Server v1.02</h1><h2>Test response</h2>\n");
	HTWriteASCII(soc, "\n\nThe (unrecognised) command used was `");
	HTWriteASCII(soc, command);
	HTWriteASCII(soc, "'.\n\n");
	
	if (TRACE) printf("Return test string written back'\n");
	free(command);
	return 0;		/* End of file - please close socket */
    } /* for */
    
} /* handle */



/*      Handle incoming messages				server_loop()
**	-------------------------
**
** On entry:
**
**      timeout         -1 for infinite, 0 for poll, else in units of 10ms
**
** On exit,
**	returns		The status of the operation, <0 if failed.
**
*/
#ifdef __STDC__
PRIVATE int server_loop(void)
#else
PRIVATE int server_loop()
#endif
{
    int tcp_status;		/* <0 if error, in general */
    int timeout = -1;		/* No timeout required but code exists */
    for(;;) {

/*  If it's a master socket, then find a slave:
*/
    	if (role == master) {
#ifdef SELECT
	    fd_set		read_chans;
	    fd_set		write_chans;
	    fd_set		except_chans;
	    int			nfound;	    /* Number of ready channels */
	    struct timeval	max_wait;   /* timeout in form for select() */
    
	    FD_ZERO(&write_chans);	    /* Clear the write mask */
	    FD_ZERO(&except_chans);	    /* Clear the exception mask */

/*  If timeout is required, the timeout structure is set up. Otherwise
**  (timeout<0) a zero is passed instead of a pointer to the struct timeval.
*/
	    if (timeout>=0) {
		max_wait.tv_sec = timeout/100;
		max_wait.tv_usec = (timeout%100)*10000;
	    }
    
	    for (com_soc=(-1); com_soc<0;) {	/* Loop while connections keep coming */
    
		
/*  The read mask expresses interest in the master channel for incoming
**  connections) or any slave channel (for incoming messages).
*/

/*  Wait for incoming connection or message
*/
	        read_chans = open_sockets;	 /* Read on all active channels */
		if (TRACE) printf(
"Daemon: Waiting for connection or message. (Mask=%x hex, max=%x hex).\n", 
		 	*(int *)(&read_chans), num_sockets);
		nfound=select(num_sockets, &read_chans,
		    &write_chans, &except_chans,
		    timeout >= 0 ? &max_wait : 0);
	
		if (nfound<0) return HTInetStatus("select()");
		if (nfound==0) return 0;	/* Timeout */

/*	We give priority to existing connected customers. When there are
**	no outstanding commands from them, we look for new customers.
*/
/*  	If a message has arrived on one of the channels, take that channel:
*/
		{
		    int i;
		    for(i=0; i<num_sockets; i++)
			if (i != master_soc)
			    if (FD_ISSET(i, &read_chans)) {
			    if (TRACE) printf(
			    	"Message waiting on socket %d\n", i);
			    com_soc = i;		/* Got one! */
			    break;
			}
		    if (com_soc>=0) break; /* Found input socket */
		    
		} /* block */
		
/*  If an incoming connection has arrived, accept the new socket:
*/
		if (FD_ISSET(master_soc, &read_chans)) {
    
			CTRACE(tfp, "Daemon: New incoming connection:\n");
			tcp_status = accept(master_soc,
					(struct sockaddr *)&soc_address,
					&soc_addrlen);
			if (tcp_status<0)
			    return HTInetStatus("accept");
			CTRACE(tfp, "Daemon: Accepted new socket %d\n",
			    tcp_status);
			FD_SET(tcp_status, &open_sockets);
			if ((tcp_status+1) > num_sockets)
				num_sockets=tcp_status+1;
    
		} /* end if new connection */
    
    
	    } /* loop on event */
	
#else	/* SELECT not supported */
    
	    if (com_soc<0) { /* No slaves: must accept */
		    CTRACE(tfp, 
		    "Daemon: Waiting for incoming connection...\n");
		    tcp_status = accept(master_soc,
				    &rsoc->mdp.soc_tcp.soc_address,
				    &rsoc->mdp.soc_tcp.soc_addrlen);
		    if (tcp_status<0)
			return HTInetStatus("accept");
		    com_soc = tcp_status;	/* socket number */
		    CTRACE(tfp, "Daemon: Accepted socket %d\n", tcp_status);
	    } /* end if no slaves */
    
#endif

	}  /* end if master */


/* com_soc is now valid for read */

	{
	    struct sockaddr_in addr;
	    int namelen = sizeof(addr);
	    getpeername(com_soc, (struct sockaddr*)&addr, &namelen);
	    strncpy(HTClientHost,
	    	 inet_ntoa(addr.sin_addr), sizeof(HTClientHost));
	}
	
/*  Read the message now on whatever channel there is:
*/
        CTRACE(tfp,"Daemon: Reading socket %d from host %s\n",
		com_soc, HTClientHost);

	tcp_status=HTHandle(com_soc);
	
	if(tcp_status<=0) {				/* EOF or error */
	    if (tcp_status<0) {				/* error */
	        CTRACE(tfp,
		"Daemon: Error %d handling incoming message (errno=%d).\n",
			 tcp_status, errno);
	        /* DONT return HTInetStatus("netread");	 error */
	    } else {
		CTRACE(tfp, "Daemon: Socket %d disconnected by peer\n",
		    com_soc);
            }
	    if (role==master) {
		NETCLOSE(com_soc);
		FD_CLR(com_soc, &open_sockets);
	    } else {  /* Not multiclient mode */
#ifdef VM
		return -69;
#else
		return -ECONNRESET;
#endif
	    }
	} else {/* end if handler left socket open */
	    NETCLOSE(com_soc);
	    FD_CLR(com_soc, &open_sockets);
        }
    }; /* for loop */
/*NOTREACHED*/
} /* end server_loop */


/*		Main program
**		------------
**
**	Options:
**	-v		verify: turn trace output on to stdout 
**	-a addr		Use different tcp port number and style
**	-l file		Log requests in ths file
**	-r file		Take rules from this file
**	-R file		Clear rules and take rules from file.
*/
int main ARGS2 (
	int,	argc,
	char**,	argv)
{
    int status;
#ifdef RULES
    int rulefiles = 0;		/* Count number loaded */
#endif
    char * addr = "";		/* default address */
    WWW_TraceFlag = 0;			/* diagnostics off by default */
#ifdef RULES
    HTClearRules();
#endif
    {
    	int a;
	    
	for (a=1; a<argc; a++) {
	    
	    if (0==strcmp(argv[a], "-v")) {
	        WWW_TraceFlag = 1;

	    } else if (0==strcmp(argv[a], "-a")) {
	        if (++a<argc) addr = argv[a];

#ifdef RULES
	    } else if (0==strcmp(argv[a], "-r")) {
	        if (++a<argc) { 
		    if (HTLoadRules(argv[a]) < 0) exit(-1);
		    rulefiles++;
		}
	    } else if (0==strcmp(argv[a], "-R")) {
		rulefiles++;		/* Inhibit rule file load */
#endif
	    } else if (0==strcmp(argv[a], "-l")) {
	        if (++a<argc) {
		    logfile = fopen(argv[a], "a");
		    log_file_name = argv[a];	/* For WAIS code */
		}
		if (!logfile) {
		    fprintf(stderr,
			"Can't open log file %s\n", argv[a]);
		    logfile = stderr;
		}
/* Remove because an inet daemon process gets started every request.
		fprintf(logfile, "\nhttpd: HTTP Daemon %s started.\n",
			 VERSION);
*/
	    } /*ifs */
	} /* for */
    } /* scope of a */
    
#ifdef RULES
    if (rulefiles==0) {		/* No mention */
        if (HTLoadRules(RULE_FILE) < 0) exit(-1);
    }
#endif
    
    
    status = do_bind(addr);
    if (status<0) {
    	fprintf(stderr, "Daemon: Bad setup: Can't bind and listen on port.\n");
    	fprintf(stderr, "     (Possibly server already running, for example).\n");
	exit(status);
    }
    
    status = server_loop();

    if (status<0) {
    	/* printf("Error in server loop.\n");  not error if inetd-started*/
	exit(status);
    }
    
    exit(0);
    return 0;		/* For gcc */
}

