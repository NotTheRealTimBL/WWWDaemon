/*		TCP/IP based server for HyperText			TCPServer.c
**		---------------------------------
**
**  History:
**	2 Oct 90	Written TBL. Include filenames for VM from RTB.
*/
/*	Module parameters:
**	-----------------
**
**  These may be undefined and redefined by syspec.h
*/
#define _NeXT_

#define LISTEN_BACKLOG 2	/* Number of pending connect requests (TCP)*/
#define MAX_CHANNELS 20		/* Number of channels we will keep open */
#define SELECT			/* Handle >1 channel if we can.		*/

#define WILDCARD '*'	    /* Wildcard used in addressing */

#define NETCLOSE close	    /* Routine to close a TCP-IP socket		*/
#define NETREAD  read	    /* Routine to read from a TCP-IP socket	*/
#define NETWRITE write	    /* Routine to write to a TCP-IP socket	*/

#define FIRST_TCP_PORT	5000	/* When using dynamic allocation */
#define LAST_TCP_PORT 5999

#ifdef NeXT
#include <libc.h>		/* NeXT has all this packaged up */
#define ntohs(x) (x)
#define htons(x) (x)
#include <streams/streams.h>

#else
#include <stdio.h>
/*	VM doesn't have a built-in predefined token, so we cheat:
#ifdef __STDIO__
#define VM
#else
#include <string.h>		/* For bzero etc - not NeXT or VM */
#endif
#endif

#include "utilities.h"		/* Coding convention macros */


#ifndef VM
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>	    /* Must be after netinet/in.h */
#include <netdb.h>
#include <errno.h>	    /* independent */
#include <sys/time.h>	    /* independent */

#else				/* VM */
#include <manifest.h>                                                           
#include <bsdtypes.h>                                                           
#include <stdefs.h>                                                             
#include <socket.h>                                                             
#include <in.h>                                                                 
extern char asciitoebcdic[], ebcdictoascii[];
#define TOASCII(c) ebcdictoascii[c]
#define FROMASCII(c) asciitoebcdic[c]                                   
#include <bsdtime.h>                                                            
#endif

/*	Default macros for manipulating masks for select()
*/
#ifndef FD_SET
typedef unsigned int fd_set;
#define FD_SET(fd,pmask) (*(pmask)) |=  (1<<(fd))
#define FD_CLR(fd,pmask) (*(pmask)) &= ~(1<<(fd))
#define FD_ZERO(pmask)   (*(pmask))=0
#define FD_ISSET(fd,pmask) (*(pmask) & (1<<(fd)))
#endif




/*	Module-Global Variables
**	-----------------------
*/
enum role_enum {master, slave, transient, passive} role;
struct sockaddr_in  soc_address;  /* See netinet/in.h 			*/
int		    soc_addrlen;
int		    master_soc;	  /* inet socket number to listen on	*/
int		    com_soc;	  /* inet socket number to read on	*/
fd_set		    open_sockets; /* Mask of channels which are active */
int		    num_sockets;  /* Number of sockets to scan */
BOOLEAN		    dynamic_allocation;	/* Search for port? */

/*	Encode INET status (as in sys/errno.h)			   inet_status()
**	------------------
**
** On entry,
**	where		gives a description of what caused the error
**	global errno	gives the error number in the unix way.
**
** On return,
**	returns		a negative status in the unix way.
*/
#ifdef vms
extern int uerrno;	/* Deposit of error info (as perr errno.h) */
extern int vmserrno;	/* Deposit of VMS error info */
extern volatile noshare int errno;  /* noshare to avoid PSECT conflict */
#else
extern int errno;
#endif
int inet_status(where)
    char    *where;
{
    CTRACE(tfp, "TCP: Error %d in `errno' after call to %s() failed.\n",
	    errno,  where);
#ifdef vms
    CTRACE(tfp, "         Unix error number (uerrno) = %ld dec\n", uerrno);
    CTRACE(tfp, "         VMS error (vmserrno)       = %lx hex\n", vmserrno);
#endif
    return -errno;
}


/*	Parse a cardinal value				        parse_cardinal()
**	----------------------
**
** On entry,
**	*pp	    points to first character to be interpreted, terminated by
**		    non 0:9 character.
**	*pstatus    points to status already valid
**	maxvalue    gives the largest allowable value.
**
** On exit,
**	*pp	    points to first unread character
**	*pstatus    points to status updated iff bad
*/
PRIVATE unsigned int parse_cardinal(pstatus, pp, max_value)
   int			*pstatus;
   char			**pp;
   unsigned int		max_value;
{
    int   n;
    if ( (**pp<'0') || (**pp>'9')) {	    /* Null string is error */
	*pstatus = -3;  /* No number where one expeceted */
	return 0;
    }

    n=0;
    while ((**pp>='0') && (**pp<='9')) n = n*10 + *((*pp)++) - '0';

    if (n>max_value) {
	*pstatus = -4;  /* Cardinal outside range */
	return 0;
    }

    return n;
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
int do_bind(const char * tsap)
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
	    return -BAD(status);
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
		sin->sin_port = htons((unsigned short)parse_cardinal(
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
		return inet_status("gethostbyname");  /* Fail? */
	    }
	    memcpy(&sin->sin_addr, phost->h_addr, phost->h_length);
	}

	CTRACE(tfp, 
	    "TCP: Parsed address as port %4x, inet %d.%d.%d.%d\n",
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
	    return inet_status("socket");
      
	CTRACE(tfp, "IP: Opened socket number %d\n", master_soc);
	
/*  If the port number was not specified, then we search for a free one.
*/
	if (dynamic_allocation) {
	    unsigned short try;
	    for (try=FIRST_TCP_PORT; try<=LAST_TCP_PORT; try++) { 
		soc_address.sin_port = htons(try);
		if (bind(master_soc,
			(struct sockaddr*)&soc_address,	/* Cast to generic sockaddr */
			sizeof(soc_address)) == 0)
		    break;
		if (try == LAST_TCP_PORT)
		    return inet_status("bind");
	    }
	    CTRACE(tfp, "IP:  Bound to port %u.\n",
		    ntohs(soc_address.sin_port));
	} else {					/* Port was specified */
	    if (bind(master_soc,
		     (struct sockaddr*)&soc_address,	/* Case to generic address */
		     sizeof(soc_address))<0)
		return inet_status("bind");
	}
	if (listen(master_soc, LISTEN_BACKLOG)<0)
	    return inet_status("listen");

	CTRACE(tfp, "TCP: Master socket(), bind() and listen() all OK\n");
	FD_SET(master_soc, &open_sockets);
	if ((master_soc+1) > num_sockets) num_sockets=master_soc+1;

	return master_soc;
    } /* if master */
    
    return -1;		/* unimplemented role */

} /* do_bind */

/*	Send a string down a socket
**	---------------------------
**
**	The trailing zero is not sent.
**	There is a maximum string length.
*/
int write_ascii(int soc, char * s)
{
    char * p;
    char ascii[255];
    for (p=s; *p; p++) {
        *ascii++ = TOASCII(*p);
    }
    return write(soc, s, p-s);
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
    char command[COMMAND_SIZE+1];
    int	status;
   
    NXStream * s = NXOpenFile(soc, NX_WRITEONLY);
    
    printf("NXOpenFile returns %i\n", s);
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
	    }
	}
#endif
	if (TRACE) printf("read string is `%s'\n", command);
	if (status<=0) return status;

	write_ascii(soc, "<h1>Small network access test</h1>\n");
	write_ascii(soc, "\n\nThe command used was `");
	write_ascii(soc, command);
	write_ascii(soc, "%s'.\n\n");
	write_ascii(soc, "Embbedded <a href=tcp:nxoc01/junk>anchor</a> here\n\n");
	
	if (TRACE) printf("Return string written back'\n");
	/* close(soc);   will be done by caller to give EOF to the other end */
	return 0;		/* End of file - please close socket */
    }
    
} /* handle */

/*      Handle incomming messages					do_action()
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
PRIVATE int do_action(void)
#else
PRIVATE int do_action()
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
    
	    for (com_soc=-1; com_soc<0;) {	/* Loop while connections keep coming */
    
		
/*  The read mask expresses interest in the master channel for incomming
**  connections) or any slave channel (for incomming messages).
*/

/*  Wait for incoming connection or message
*/
	        read_chans = open_sockets;	    /* Read on all active channels */
		if (TRACE) printf(
	"TcpAccess: Waiting for connection or message. (Mask=%x hex, max=%x hex).\n", 
		 	*(int *)(&read_chans), num_sockets);
		nfound=select(num_sockets, &read_chans,
		    &write_chans, &except_chans,
		    timeout >= 0 ? &max_wait : 0);
	
		if (nfound<0) return inet_status("accept");
		if (nfound==0) return 0;	/* Timeout */

/*  If an incomming connection has arrived, accept the new socket:
*/
		if (FD_ISSET(master_soc, &read_chans)) {
    
			CTRACE(tfp, "TCP: New incomming connection:\n");
			tcp_status = accept(master_soc,
					(struct sockaddr *)&soc_address,
					&soc_addrlen);
			if (tcp_status<0)
			    return inet_status("accept");
			CTRACE(tfp, "TCP: Accepted new socket %d\n",
			    tcp_status);
			FD_SET(tcp_status, &open_sockets);
			if ((tcp_status+1) > num_sockets) num_sockets=tcp_status+1;
			nfound--;
    
		} /* end if new connection */
    
/*  If a message has arrived on one of the channels, take that channel:
*/
		if(nfound) {
			int i;
			for(i=0;;i++)
			    if(FD_ISSET(i, &read_chans)) {
				if (TRACE) printf("Got new socket %i\n", i);
				com_soc = i;		/* Got one! */
				break;
			    }
			
			break;			/* Found input socket com_soc */
		} /* if message waiting */
    
	    } /* loop on event */
	
#else	/* SELECT not supported */
    
	    if (com_soc<0) { /* No slaves: must accept */
		    CTRACE(tfp, "TCP: Waiting for incomming connection...\n");
		    tcp_status = accept(master_soc,
				    &rsoc->mdp.soc_tcp.soc_address,
				    &rsoc->mdp.soc_tcp.soc_addrlen);
		    if (tcp_status<0)
			return inet_status("accept");
		    com_soc = tcp_status;	/* socket number */
		    CTRACE(tfp, "TCP: Accepted socket %d\n", tcp_status);
	    } /* end if no slaves */
    
#endif

	}  /* end if master */


/* com_soc is now valid for read */

/*  Read the message now on whatever channel there is:
*/
        CTRACE(tfp,"TCP: Reading socket %d\n", com_soc);
	tcp_status=handle(com_soc);
	
	if(tcp_status<=0) {				/* EOF or error */
	    if (tcp_status<0) {				/* error */
	        CTRACE(tfp, "TCP: Error handling incomming message.\n");
	        /* DONT return inet_status("netread");	 error */
	    } else {
		CTRACE(tfp, "TCP: Socket %d disconnected by peer\n",
		    com_soc);
            }
	    if (role==master) {
		NETCLOSE(com_soc);
		FD_CLR(com_soc, &open_sockets);
	    } else
		return -ECONNRESET;
	} /* end if handler got EOF or error */
    
    }; /* for loop */
/*NOTREACHED*/
} /* end do_action */


/*		Main program
**		------------
*/
int main(int arc, char*argv[])
{
    int status;
    
    status = do_bind("*:2784");
    if (status<0) {
    	printf("Bad setup.\n");
	exit(status);
    }
    
    status = do_action();
    if (status<0) {
    	printf("Error in server loop.\n");
	exit(status);
    }
    
    exit(0);
    return 0;		/* For gcc */
}

