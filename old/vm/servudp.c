/* Simple server waiting for connections and printing all messages read         
*/                                                                              
#include <stdio.h>                                                              
                                                                                
#ifdef __STDIO__                                                                
#define IBM                                                                     
#endif                                                                          
                                                                                
#define MY_SOCKET 0x4242                                                        
#ifdef IBM                                                                      
#include <types.h>                                                              
#include <x11glue.h>                                                            
#include <socket.h>                                                             
#include <in.h>                                                                 
#define u_long ulong                                                            
                                                                                
#else                                                                           
                                                                                
#include <sys/types.h>                                                          
#include <sys/socket.h>                                                         
#include <netinet/in.h>                                                         
                                                                                
#endif   /* not on IBM */                                                       
                                                                                
                                                                                
#include <netdb.h>                                                              
#include <time.h>                                                               
                                                                                
main()                                                                          
{                                                                               
int sock, length;                                                               
struct sockaddr_in server;                                                      
struct sockaddr_in client;                                                      
static int client_l=sizeof(client);                                             
int msgsock;                                                                    
char buf[1024];                                                                 
int rc;                                                                         
unsigned long ready;                                                            
unsigned long writy;                                                            
unsigned long excpy;                                                            
unsigned long mask;                                                             
                                                                                
struct tm to;                                                                   
                                                                                
#ifdef IBM                                                                      
tcp_debug(1);                                                                   
#endif                                                                          
                                                                                
/* create socket */                                                             
sock=socket(AF_INET, SOCK_DGRAM, 0);                                            
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
                                                                                
#if 0                                                                           
/* find out assigned port number and print it */                                
length=sizeof(server);                                                          
if (getsockname(sock, &server, &length))                                        
  { perror("getting socket name");                                              
    exit(1);                                                                    
  }                                                                             
#endif                                                                          
printf("Socket has port #%d\n", ntohs(server.sin_port));                        
                                                                                
/* start accepting connections */                                               
mask=1<<sock;                                                                   
printf("socket is %d mask is %x\n", sock, mask);                                
                                                                                
                                                                                
to.tm_sec=60;      /* timeout 60 seconds */                                     
                                                                                
for (;/*ever*/;)                                                                
  {                                                                             
    ready=mask;                                                                 
    if (select(sock+1, &ready, 0, 0, &to)<0)                                    
      { perror("select");                                                       
        continue;                                                               
      }                                                                         
    printf("Mask after select: %x mask %x ready&mask %x\n",                     
           ready, mask, ready&mask);                                            
    if (ready&mask)                                                             
      {                                                                         
        rc=recvfrom(sock, buf, sizeof(buf), 0, &client, &client_l);             
        printf("recvfrom rc %d\n", rc);                                         
        if(rc<0)                                                                
          perror("recvfrom");                                                   
        else {                                                                  
          buf[rc]='\0';                                                         
          puts(buf);                                                            
        }                                                                       
        if (buf[0]=='q') break;                                                 
      }                                                                         
    else                                                                        
      printf("Do something else\n");                                            
  }                                                                             
close(sock);                                                                    
                                                                                
                                                                                
} /* End of main program */                                                     
                                                                                
#if 0                                                                           
int dump_bfr()                                                                  
{}                                                                              
#endif                                                                          
