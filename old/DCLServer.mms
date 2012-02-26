!	Make WorldWideWeb server for VMS Help
!       =====================================
!
! History:
!  26 Feb 92 (JFG)	Written from the line-mode browser's DESCRIP.MMS
! Bugs:
!	Please report to www-bug@info.cern.ch
! Instructions:
!	Use the correct command line for your TCP/IP implementation:
!
!	$ MMS/MACRO=(MULTINET=1)	for Multinet
!	$ MMS/MACRO=(WIN_TCP=1)		for Wollongong TCP/IP
!	$ MMS/MACRO=(UCX=1)		for DEC/UCX
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

.IFDEF LIBS
.ELSE
LIBS = multinet.opt/opt			! (Default to multinet)
OPTION_FILE = multinet.opt
CFLAGS = /DEFINE=(DEBUG,MULTINET)
.ENDIF

OBJECTS = HTDaemon.obj, HTString.obj, HTTCP.obj, DocDBGate.obj

OBJECTS_D = HTDaemon_d.obj, HTString_d.obj, HTTCP_d.obj, DocDBGate_d.obj

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
!_____________________________	DocDBGate

DocDBGate.obj   : DocDBGate.c HTString.h HTUtils.h tcp.h
        cc $(CFLAGS)/lis/obj=$*.obj DocDBGate.c
DocDBGate_d.obj : DocDBGate.c HTString.h HTUtils.h tcp.h
	cc/debug $(CFLAGS)/obj=$*.obj DocDBGate.c
.IFDEF U
DocDBGate.c : $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/DocDBGate.c"
	     copy $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/DocDBGate.c" - 
             DocDBGate.c
.ENDIF

! _____________________________VMS SPECIAL FILES:

.IFDEF U
! latest version of this one:
descrip.mms : $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/VMSHelp/descrip.mms"
	copy $(U)"/userd/tbl/hypertext/WWW/Daemon/Implementation/VMSHelp/descrip.mms" -
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


