/* Network Virtual disk daemon                                                  
                                                                                
   Author: R.Toebbicke, CERN DD                                                 
*/                                                                              
#define DEBUG                                                                   
#include <stdio.h>                                                              
#include <string.h>                                                             
                                                                                
#ifdef __STDIO__                                                                
#define VM                                                                      
#endif                                                                          
                                                                                
#ifdef VM                                                                       
#include <manifest.h>                                                           
#include <bsdtypes.h>                                                           
#include <stdefs.h>                                                             
#include <socket.h>                                                             
#include <in.h>                                                                 
#define MY_SOCKET 8018                                                          
                                                                                
extern char asciitoebcdic[], ebcdictoascii[];                                   
#include <bsdtime.h>                                                            
                                                                                
#else                                                                           
                                                                                
#include <sys/types.h>                                                          
#include <sys/socket.h>                                                         
#include <netinet/in.h>                                                         
#define MY_SOCKET 8018                                                          
#include <netdb.h>                                                              
#include <time.h>                                                               
                                                                                
#endif   /* not on VM */                                                        
                                                                                
#include "rtbaux.h"                                                             
                                                                                
#ifdef VM                                                                       
struct FSCB                                                                     
  { /* FSCB structure for CMS */                                                
    char   comm[8];                                                             
    char   fn[8];                                                               
    char   ft[8];                                                               
    char   fm[2];                                                               
    short  itno;                                                                
    char   *buff;                                                               
    long   size;                                                                
    char   fv;          /* record format */                                     
    char   flag;        /* flags */                                             
#define FSCBEPL 0x20    /* Extended Plist */                                    
    short  noit;        /* number of items */                                   
    long   nord;        /* bytes read */                                        
    long   aitn;        /* Alternative item number */                           
    long   anit;        /* Alternative number of items */                       
    long   wptr;        /* Alternative read pointer */                          
    long   rptr;        /* Alternative write pointer */                         
  };                                                                            
#endif                                                                          
                                                                                
                                                                                
#define SECSIZE 512                                                             
                                                                                
struct _client                                                                  
  {                                                                             
    struct _client *next;                                                       
    int sock;                                                                   
    int state;                                                                  
#define CL_NEW 0                                                                
#define CL_RECV 1                                                               
#define CL_XMIT 2                                                               
    long mask;                                                                  
    struct sockaddr_in address;                                                 
    int ssn;                                                                    
    int cnt;                                                                    
                                                                                
#ifdef VM                                                                       
    /* The following must be kept together for VM */                            
    struct FSCB fscb;                                                           
#endif                                                                          
                                                                                
                                                                                
    char bf[4096];                                                              
    char *currbf;                                                               
    int  bflen;                                                                 
                                                                                
                                                                                
                                                                                
  } ;                                                                           
                                                                                
                                                                                
struct _client *cl;                                                             
struct _client *cl_anchor;                                                      
                                                                                
typedef                                                                         
  struct                                                                        
    {                                                                           
      short ssn;        /* Starting sector high/low byte */                     
      short cnt;        /* Count high/low byte */                               
      char func;        /* Function */                                          
#define      RQOPEN     0x03    /* Open */                                      
#define      RQWRITE    0x01    /* Write */                                     
#define      RQREAD     0x02    /* Read */                                      
#define      RQBOOT     0x07    /* Read Boot record */                          
      char rsvd[3];     /* Reserved */                                          
    } RQPARM;                                                                   
                                                                                
                                                                                
                                                                                
struct _client *new_client();                                                   
unsigned long ctl_mask, mask;                                                   
                                                                                
#define SEC_READ 0                                                              
#define SEC_WRITE 1                                                             
                                                                                
int trace=0;                                                                    
                                                                                
                                                                                
main(argc, argv)                                                                
  int argc;                                                                     
  char **argv;                                                                  
{                                                                               
int sock, length;                                                               
struct sockaddr_in server;                                                      
static int client_l=sizeof(struct sockaddr_in);                                 
int rc;                                                                         
unsigned long ready=0;                                                          
unsigned long writy=0;                                                          
unsigned long excpy=0;                                                          
                                                                                
int i;                                                                          
                                                                                
struct timeval to;                                                              
                                                                                
if (argc>1)                                                                     
  trace=*argv[1]=='t';                                                          
                                                                                
                                                                                
#ifdef DEBUG                                                                    
if (trace) tcp_debug(1);                                                        
#endif                                                                          
                                                                                
                                                                                
/* create socket */                                                             
sock=socket(AF_INET, SOCK_STREAM, 0);                                           
if (sock<0)                                                                     
  { perror("opening stream socket");                                            
    exit(1);                                                                    
  }                                                                             
                                                                                
/* Name socket using wildcards */                                               
server.sin_family=AF_INET;                                                      
                                                                                
server.sin_addr.s_addr=INADDR_ANY;                                              
server.sin_port=ntohs(MY_SOCKET);                                               
if (bind(sock, &server, sizeof(server)))                                        
  { perror("binding stream socket");                                            
    exit(1);                                                                    
  }                                                                             
                                                                                
printf("Socket has port #%d\n", ntohs(server.sin_port));                        
                                                                                
/* start accepting connections */                                               
mask=1<<sock;                                                                   
ctl_mask=mask;                                                                  
                                                                                
listen(sock,5);                                                                 
                                                                                
to.tv_sec=300;      /* timeout interval */                                      
                                                                                
for (;/*ever*/;)                                                                
  {                                                                             
    ready=mask;                                                                 
    if (select(32, &ready, 0, 0, &to)<0)                                        
      { perror("select");                                                       
        continue;                                                               
      }                                                                         
#ifdef DEBUG                                                                    
    if (trace)                                                                  
    printf("Masks after select (r/w/x): %x %x %x mask %x\n",                    
           ready, writy, excpy, mask);                                          
#endif                                                                          
                                                                                
    if (ready&ctl_mask)      /* Got a new client */                             
      { /* Allocate a new client */                                             
        if ( (cl=new_client())!=(struct _client *) 0)                           
          {                                                                     
            cl->sock=accept(sock, &(cl->address), &client_l);                   
            printf("accept: sock=%d\n", cl->sock);                              
            if (cl->sock==-1)                                                   
              {                                                                 
                perror("accept");                                               
                drop_client(cl);                                                
              }                                                                 
            else                                                                
              {                                                                 
                cl->mask=1 << (cl->sock);                                       
                mask|=cl->mask;                                                 
#ifdef DEBUG                                                                    
                if (trace)                                                      
                printf("This mask %x new mask: %x\n", cl->mask, mask);          
#endif                                                                          
              }                                                                 
          }                                                                     
        continue;                                                               
      }                                                                         
                                                                                
    /* Scan for data ready */                                                   
    for (cl=cl_anchor; cl; cl=cl->next)                                         
      {                                                                         
        if (cl->sock!=-1)                                                       
          if ( ready & (cl->mask) )                                             
            {                                                                   
#ifdef DEBUG                                                                    
              if (trace)                                                        
              printf("Client %d (sock %d) ready\n", i, cl->sock);               
#endif                                                                          
              read_client(cl);                                                  
            }                                                                   
      }                                                                         
                                                                                
  }                                                                             
                                                                                
} /* End of main program */                                                     
                                                                                
struct _client *                                                                
new_client()                                                                    
  {                                                                             
                                                                                
    struct _client *cl;                                                         
    cl=(struct _client *) malloc(sizeof(struct _client));                       
                                                                                
    cl->next=cl_anchor;                                                         
    cl_anchor=cl;                                                               
                                                                                
    cl->currbf=cl->bf;       /* place for receive */                            
    cl->bflen=0;             /* current buffer empty */                         
                                                                                
    cl->state=CL_NEW;        /* Waiting for a request */                        
                                                                                
    mvc(cl->fscb.fn,"TCPDISK ",8);                                              
    mvc(cl->fscb.ft,"VDISK   ",8);                                              
    mvc(cl->fscb.fm,"A ",2);                                                    
    cl->fscb.fv='F';                                                            
                                                                                
    return(cl);                                                                 
  }                                                                             
                                                                                
                                                                                
drop_client(cl)                                                                 
  struct _client * cl;                                                          
{                                                                               
  struct _client *pre;                                                          
                                                                                
  if ((cl->sock)!=-1)                                                           
    {                                                                           
      close(cl->sock);                                                          
      mask&=~cl->mask;                                                          
      printf("Deleted sock %d, new mask %x\n", cl->sock, mask);                 
    }                                                                           
                                                                                
  if (cl==cl_anchor)                                                            
    cl_anchor=cl->next;                                                         
  else                                                                          
    for (pre=cl_anchor; pre; pre=pre->next)                                     
      if (pre->next==cl)                                                        
        pre->next=cl->next;                                                     
                                                                                
  free(cl);                                                                     
                                                                                
}                                                                               
                                                                                
read_client(cl)                                                                 
  struct _client * cl;                                                          
{                                                                               
  int rc;                                                                       
  char *start;                                                                  
                                                                                
  start=cl->currbf+cl->bflen;                                                   
  rc=read(cl->sock, start, cl->bf+sizeof(cl->bf)-start);                        
#ifdef DEBUG                                                                    
  if (trace)                                                                    
  printf("read: rc=%d\n", rc);                                                  
#endif                                                                          
  if (rc<0)                                                                     
    { char buf[256];                                                            
      sprintf(buf, "Reading from socket %d errno %d: ",                         
              cl->sock, errno);                                                 
      perror(buf);                                                              
    }                                                                           
  else                                                                          
    if (rc==0)                                                                  
      drop_client(cl);                                                          
    else                                                                        
      {                                                                         
        cl->bflen+=rc;                                                          
        cl_data(cl);                                                            
      }                                                                         
                                                                                
  while ( cl->bflen >=                                                          
             ( cl->state==CL_NEW ? sizeof(RQPARM) : SECSIZE)  )                 
    cl_data(cl);                                                                
                                                                                
                                                                                
}                                                                               
                                                                                
#ifdef DEBUG                                                                    
mydump(b,l)                                                                     
  char * b;                                                                     
  int l;                                                                        
{                                                                               
  char t[17];                                                                   
  int i;                                                                        
  char c;                                                                       
                                                                                
  for (; l>0; b+=16, l-=16)                                                     
    {                                                                           
                                                                                
      for (i=0; i<16; i++)                                                      
        {                                                                       
#ifdef VM                                                                       
          c=asciitoebcdic[b[i]];                                                
#else                                                                           
          c=b[i];                                                               
#endif                                                                          
          t[i]=(isprint(c) ? c : '.');                                          
        };                                                                      
                                                                                
#define l(n) ((long *)b)[n]                                                     
      printf("%08x %08x  %08x %08x", l(0), l(1), l(2), l(3));                   
#if   1                                                                         
      printf("  | %-.16s |\n", t);                                              
#else                                                                           
      putchar('\n');                                                            
#endif                                                                          
#undef l                                                                        
                                                                                
    }                                                                           
                                                                                
}                                                                               
#endif                                                                          
                                                                                
                                                                                
int                                                                             
cl_data(cl)                                                                     
  struct _client *cl;                                                           
{                                                                               
  RQPARM *rq;                                                                   
  int rc;                                                                       
                                                                                
#ifdef DEBUG                                                                    
  if (trace)                                                                    
    printf("Client socket %d state '%s' current data %d bytes\n",               
            cl->sock,                                                           
            cl->state==CL_NEW ? "new" : cl->state==CL_RECV ? "recv"             
                   : "send",                                                    
            cl->bflen);                                                         
#endif                                                                          
  if (cl->state==CL_RECV)                                                       
    {                                                                           
      if (cl->bflen>=SECSIZE)                                                   
        wrsec(cl);                                                              
    }                                                                           
  else                                                                          
  if (cl->state==CL_NEW)                                                        
    if (cl->bflen>=sizeof(RQPARM))                                              
      {                                                                         
         rq=(RQPARM *) cl->currbf;                                              
                                                                                
         cl->ssn=rq->ssn;                                                       
         cl->cnt=rq->cnt;                                                       
         cl->bflen-=sizeof(RQPARM);                                             
         cl->currbf=cl->bflen ? cl->currbf+sizeof(RQPARM) : cl->bf;             
#ifdef DEBUG                                                                    
         if (trace)                                                             
         printf("Function 0x%x ssn %d cnt %d\n",                                
                rq->func, cl->ssn, cl->cnt);                                    
#endif                                                                          
         do_rq(cl, rq->func);                                                   
                                                                                
                                                                                
      }                                                                         
                                                                                
                                                                                
}                                                                               
                                                                                
do_rq(cl,f)                                                                     
  struct _client *cl;                                                           
  int f;                                                                        
{                                                                               
  int rc;                                                                       
  switch(f)                                                                     
    {                                                                           
      case RQREAD:                                                              
        cl->state=CL_XMIT;                                                      
        readsec(cl);                                                            
        break;                                                                  
                                                                                
      case RQWRITE:                                                             
        cl->state=CL_RECV;                                                      
        if (cl->bflen>=SECSIZE)                                                 
          wrsec(cl);                                                            
        break;                                                                  
                                                                                
      case RQOPEN:                                                              
        {                                                                       
          struct { char *r14, *r15, *r0, *r1; } _regs20x;                       
          xplist xp;                                                            
          char tbuf[256];                                                       
          static char execcmd[]="EXEC    TCPCTL  ";                             
          extern _svc20x();                                                     
          xp.comverb=execcmd;                                                   
#define EXECNAME "TCPCTL  "                                                     
#define EXECSIZE sizeof(EXECNAME)-1                                             
          mvc(tbuf, EXECNAME, EXECSIZE);                                        
          if (!cl->bflen)                                                       
            {                                                                   
              cl->bflen=read(cl->sock, cl->currbf, 256);                        
#ifdef DEBUG                                                                    
              if (trace)                                                        
                printf("%d bytes of control data\n", cl->bflen);                
#endif                                                                          
            }                                                                   
          if (cl->bflen<=0)                                                     
            printf("Unable to read open control data\n");                       
          else                                                                  
            {                                                                   
              char *getenv();                                                   
              memcpy(tbuf+EXECSIZE, cl->currbf,                                 
                    (cl->bflen<sizeof(tbuf)-EXECSIZE) ? cl->bflen :             
                    sizeof(tbuf)-EXECSIZE);                                     
              xp.begargs=tbuf;                                                  
              xp.endargs=xp.begargs+EXECSIZE+cl->bflen;                         
              xp.fblok=(char *)0;                                               
              _regs20x.r0=(char *) &xp;                                         
              _regs20x.r1=(char *)                                              
                    ((int)execcmd | FLAG202_XPLIST);                            
              _svc20x(& _regs20x);                                              
#ifdef DEBUG                                                                    
              if (trace) printf("svc20x returns R15=%d\n",                      
                                _regs20x.r15);                                  
#endif                                                                          
              mvc(cl->fscb.fn,getenv("FSCBFN"),8);                              
              mvc(cl->fscb.ft,getenv("FSCBFT"),8);                              
              mvc(cl->fscb.fm,getenv("FSCBFM"),2);                              
#ifdef DEBUG                                                                    
              if (trace) printf("fn ft fm: %.8s %.8s %.2s\n",                   
                      cl->fscb.fn, cl->fscb.ft, cl->fscb.fm);                   
#endif                                                                          
                                                                                
            }                                                                   
        }                                                                       
        cl->currbf=cl->bf;                                                      
        cl->bflen=0;                                                            
        break;                                                                  
                                                                                
      case RQBOOT:                                                              
        secio(cl, SEC_READ, 0, 1, cl->bf);                                      
        if (write(cl->sock, cl->bf, 24)!=24)                                    
          perror("Boot read");                                                  
        cl->currbf=cl->bf;                                                      
        cl->bflen=0;                                                            
        break;                                                                  
                                                                                
      default:                                                                  
        printf("Error in RQPARM func %x, %d bytes flushed\n",                   
               f, cl->bflen);                                                   
        cl->currbf=cl->bf;                                                      
        cl->bflen=0;                                                            
    }                                                                           
}                                                                               
                                                                                
int                                                                             
secio(cl, cmd, start, cnt, bf)                                                  
  struct _client *cl;                                                           
  int cmd;                                                                      
  unsigned int start, cnt;                                                      
  char *bf;                                                                     
{                                                                               
  struct { char *r14, *r15, *r0, *r1; } _regs20x;                               
  char *ccmd= cmd==SEC_READ ? "RDBUF   " : "WRBUF   ";                          
                                                                                
  mvc(cl->fscb.comm, ccmd, 8);                                                  
  cl->fscb.buff=bf;                                                             
  cl->fscb.size=cmd==SEC_READ ? sizeof(cl->bf) : cnt*SECSIZE;                   
  cl->fscb.itno=start+1;                                                        
  cl->fscb.noit=cnt;                                                            
  cl->fscb.flag=0;                                                              
  _regs20x.r1=(char *) &cl->fscb.comm;                                          
  _svc20x(& _regs20x);                                                          
#ifdef DEBUG                                                                    
   if (trace)                                                                   
     printf("command '%s' rec %d cnt %d size %d nord %d rc %d (0x%x)\n",        
            cl->fscb.comm, cl->fscb.itno, cl->fscb.noit, cl->fscb.size,         
            cl->fscb.nord, _regs20x.r15, _regs20x.r15);                         
#endif                                                                          
  _regs20x.r1=(char *) cl->fscb.comm;                                           
                                                                                
  if (_regs20x.r15) return( -(int)_regs20x.r15);                                
  else return( cmd==SEC_READ ? cl->fscb.nord : 0);                              
}                                                                               
                                                                                
                                                                                
readsec(cl)                                                                     
  struct _client *cl;                                                           
{                                                                               
  int secs, rc;                                                                 
  int l;                                                                        
                                                                                
  while(cl->cnt>0)                                                              
    {                                                                           
      secs=sizeof(cl->bf)/SECSIZE;                                              
      if (secs>cl->cnt) secs=cl->cnt;                                           
      rc=secio(cl, SEC_READ, cl->ssn, secs, cl->bf);                            
#ifdef DEBUG                                                                    
      if (trace)                                                                
      printf("Sector read (%d for %d) rc=%d\n", cl->ssn, secs, rc);             
#endif                                                                          
      l=secs*SECSIZE;                                                           
      rc=write(cl->sock, cl->bf, secs*SECSIZE);                                 
#ifdef DEBUG                                                                    
      if (trace || (rc!=l))                                                     
      printf("... Write rc=%d\n",rc);                                           
#endif                                                                          
      cl->ssn+=secs;                                                            
      cl->cnt-=secs;                                                            
    }                                                                           
  cl->state=CL_NEW;                                                             
  cl->currbf=cl->bf;                                                            
                                                                                
}                                                                               
                                                                                
wrsec(cl)                                                                       
  struct _client *cl;                                                           
{                                                                               
                                                                                
  int length;                                                                   
                                                                                
  length=cl->bflen/SECSIZE;                                                     
  if (length>cl->cnt) length=cl->cnt;                                           
                                                                                
  secio(cl, SEC_WRITE, cl->ssn, length, cl->currbf);                            
                                                                                
  cl->ssn+=length;                                                              
  cl->cnt-=length;                                                              
  length*=SECSIZE;                                                              
  cl->bflen-=length;                                                            
  if (cl->bflen)                                                                
    {                                                                           
      memcpy(cl->bf, cl->currbf+length, cl->bflen);                             
#ifdef DEBUG                                                                    
      if (trace)                                                                
        printf("Buffer readjust, %d bytes copied from %x to %x\n",              
               cl->bflen, cl->currbf+length, cl->bf);                           
#endif                                                                          
    }                                                                           
  cl->currbf=cl->bf;                                                            
                                                                                
  if (!cl->cnt) cl->state=CL_NEW;                                               
                                                                                
}                                                                               
