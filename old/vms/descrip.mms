!	Make WorldWideWeb server for VMS
!       ================================
!
! History:
!  26 Feb 92 (JFG)	Written from the line-mode browser's DESCRIP.MMS
!  25 Jun 92 (JFG)	Added TCP socket emulation over DECnet
!
! Bugs:
!	Please report to www-bug@info.cern.ch
! Instructions:
!	Use the correct command line for your TCP/IP implementation:
!
!	$ MMS/MACRO=(MULTINET=1)	for Multinet
!	$ MMS/MACRO=(WIN_TCP=1)		for Wollongong TCP/IP
!	$ MMS/MACRO=(UCX=1)		for DEC/UCX
!	$ MMS/MACRO=(DECNET=1)		for socket emulation over DECnet
!
! If you are on HEP net and want to build using the really latest sources on
! PRIAM:: then define an extra macro U=PRIAM::, e.g.
!
!	$ MMS/MACRO=(MULTINET=1, U=PRIAM::)	for Multinet
!
! This will copy the sources from PRIAM as necessary. You can also try
!
!	$ MMS/MACRO=(U=PRIAM::) descrip.mms
!
! to update this file.
!
.IFDEF UCX
LIBS = sys$library:ucx$ipc/lib		! For UCX
OPTION_FILE =
CFLAGS = /DEBUG/DEFINE=DEBUG
.ENDIF
.IFDEF MULTINET
LIBS = multinet.opt/opt			! For Multinet
OPTION_FILE = multinet.opt
CFLAGS = /DEFINE=(DEBUG,MULTINET)
.ENDIF
.IFDEF WIN_TCP
LIBS = win_tcp.opt/opt			! For Wollongong TCP
OPTION_FILE = win_tcp.opt
CFLAGS = /DEFINE=(DEBUG,WIN_TCP)
.ENDIF
.IFDEF DECNET
LIBS =  disk$c3:[hemmer.unix.usr.lib]libc.opt/opt	! TCP socket library over DECnet
OPTION_FILE = disk$c3:[hemmer.unix.usr.lib]libc.opt
CFLAGS = /DEFINE=(DEBUG,DECNET)
.ENDIF

.IFDEF LIBS
.ELSE
LIBS = multinet.opt/opt			! (Default to multinet)
OPTION_FILE = multinet.opt
CFLAGS = /DEFINE=(DEBUG,MULTINET)
.ENDIF

OBJECTS = HTDaemon.obj, HTString.obj, HTTCP.obj, HTRetrieve.obj, HTParse.obj

OBJECTS_D = HTDaemon_d.obj, HTString_d.obj, HTTCP_d.obj, HTRetrieve_d.obj, HTParse_d.obj

!___________________________________________________________________
! WWW_Server

WWW_Server.exe :  $(OBJECTS) $(OPTION_FILE) descrip.mms
	define/nolog LNK$LIBRARY SYS$LIBRARY:VAXCRTL.OLB
	link/exe=WWW_Server $(OBJECTS), $(LIBS)

WWW_Server_d.exe :  $(OBJECTS_D) $(OPTION_FILE) descrip.mms
	define/nolog LNK$LIBRARY SYS$LIBRARY:VAXCRTL.OLB
	link/debug/exe=WWW_Server_d $(OBJECTS_D), $(LIBS)

!____________________________________________________________________

update : www.exe setup.com
	copy www.exe [.works]
	copy setup.com [.works]

!____________________________________________________________________
!    C O M M O N	M O D U L E S

!________________________________ HTString

HTString.obj   : HTString.c HTString.h HTUtils.h tcp.h
        cc $(CFLAGS)/obj=$*.obj HTString.c
HTString_d.obj : HTString.c HTString.h HTUtils.h tcp.h
	cc/debug $(CFLAGS)/obj=$*.obj HTString.c
.IFDEF U
HTString.c  : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTString.c"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTString.c" - 
             HTString.c
HTString.h  : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTString.h"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTString.h" -
             HTString.h
.ENDIF

!________________________________ HTParse

.IFDEF U
HTParse.c  : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTParse.c"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTParse.c" - 
             HTParse.c
HTParse.h  : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTParse.h"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTParse.h" -
             HTParse.h
.ENDIF
HTParse.obj   : HTParse.c HTParse.h HTUtils.h tcp.h
        cc $(CFLAGS)/obj=$*.obj HTParse.c
HTParse_d.obj : HTParse.c HTParse.h HTUtils.h tcp.h
	cc/debug $(CFLAGS)/obj=$*.obj HTParse.c

! _________________________________ HTTCP

HTTCP.obj : HTTCP.c HTString.h HTTCP.h HTUtils.h tcp.h
         cc $(CFLAGS)/obj=$*.obj HTTCP.c
HTTCP_d.obj : HTTCP.c HTString.h HTTCP.h HTUtils.h tcp.h
         cc/debug $(CFLAGS)/obj=$*.obj HTTCP.c
.IFDEF U
HTTCP.c    : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTTCP.c"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTTCP.c" - 
             HTTCP.c
HTTCP.h    : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTTCP.h"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTTCP.h" - 
             HTTCP.h
.ENDIF
!_________________________________ include files only:

.IFDEF U
HTUtils.h  : $(U)"/userd/tbl/hypertext/WWW/Implementation/HTUtils.h"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/HTUtils.h" -
             HTUtils.h
tcp.h      : $(U)"/userd/tbl/hypertext/WWW/Implementation/tcp.h"
	     copy $(U)"/userd/tbl/hypertext/WWW/Implementation/tcp.h" - 
             tcp.h
.ENDIF
!____________________________________________________________________________
!	S E R V E R	M O D U L E S
 
!_____________________________	HTDaemon

HTDaemon.obj   : HTDaemon.c HTString.h HTTCP.h HTUtils.h tcp.h
        cc $(CFLAGS)/obj=$*.obj HTDaemon.c
HTDaemon_d.obj : HTDaemon.c HTString.h HTTCP.h HTUtils.h tcp.h
	cc/debug $(CFLAGS)/obj=$*.obj HTDaemon.c
.IFDEF U
HTDaemon.c : $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/HTDaemon.c"
	     copy $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/HTDaemon.c" - 
             HTDaemon.c
.ENDIF
!_____________________________	HTRetrieve

HTRetrieve.obj   : HTRetrieve.c HTParse.h HTString.h HTUtils.h tcp.h HTFormat.h WWW.h
        cc $(CFLAGS)/obj=$*.obj HTRetrieve.c
HTRetrieve_d.obj : HTRetrieve.c HTParse.h HTString.h HTUtils.h tcp.h HTFormat.h WWW.h
	cc/debug $(CFLAGS)/obj=$*.obj HTRetrieve.c
.IFDEF U
HTRetrieve.c : $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/HTRetrieve.c"
	     copy $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/HTRetrieve.c" - 
             HTRetrieve.c
.ENDIF

! _____________________________VMS SPECIAL FILES:

.IFDEF U
! latest version of this one:
descrip.mms : $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/vms/descrip.mms"
	copy $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/vms/descrip.mms" -
	descrip.mms
	write sys$output "DESCRIP.MMS was updated. Please re-launch MMS..."
	exit

multinet.opt  : $(U)"/userd/tbl/hypertext/WWW/LineMode/Implementation/vms/multinet.opt"
	     copy $(U)"/userd/tbl/hypertext/WWW/LineMode/Implementation/vms/multinet.opt" - 
             multinet.opt
win_tcp.opt  : $(U)"/userd/tbl/hypertext/WWW/LineMode/Implementation/vms/win_tcp.opt"
	     copy $(U)"/userd/tbl/hypertext/WWW/LineMode/Implementation/vms/win_tcp.opt" - 
             win_tcp.opt

.ENDIF


