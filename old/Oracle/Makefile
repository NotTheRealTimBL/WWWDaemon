#		Make WorldWideWeb daemons	-- NeXT
#
# To port to another machine, you may have to alter some of the defienitions
# of this Makefile a little.
#
# For the WAIS gateway, you need a WAIS release at the same directory
# level as WWW or else set the pointer WAIS below to point to it.
# Also, WBIN should be [linked to] the WAIS bin
# directory for this machine.
#
INC = ../../../Implementation
CMN = ../../../Implementation
SRC = ..

WAIS = ../../../../WAIS
WIR = $(WAIS)/ir
WBIN = bin

# WAIS library for WAIS b3.
WAISLIB = $(WBIN)/ui.a

# WAIS library for WAIS-b4 (maybe not alll needed)
#WAISLIB =   $(WBIN)/client.a $(WBIN)/inv.a \
#	$(WBIN)/wais.a $(WBIN)/libftw.a $(WBIN)/sig.a

CFLAGS = -Wall -I$(INC) -DDEBUG -g
CC = cc
LFLAGS =  -lNeXT_s -lsys_s

#	Common Makefile for WWW Servers and Gateways
#	--------------------------------------------
#
# A Makefile in a subdirectory for the machine in question contains
# the necessary definitions of directories etc.
#
# make httpd		The W3 http daemon
# 26 Jun 92	A few NeXTisms removed to the Makefile (TBL)
#		By default, only build httpd to not confuse people
#
OBJS = HTDaemon.o HTRetrieve.o HTRules.o HTTCP.o  \
	HTParse.o HTString.o
FOBJS = HTDaemonNR.o FINDGate.o HTTCP.o HTParse.o HTString.o
WOBJS = HTDaemonNR.o WAISGate.o HTTCP.o HTParse.o ParseWSRC.o HTString.o
TWOBJS = test.o WAISGate.o HTTCP.o HTParse.o HTString.o

include $(SRC)/Version.make

all : httpd
	echo "httpd daemon up to date"

#all : httpd WAISGate
#	echo "httpd and WAISGate daemons up to date"

tar : httpd
	tar 
#		Hypertext File server

httpd : $(OBJS)
	$(CC) $(LFLAGS) -o httpd $(OBJS)
HTRetrieve.o : HTRetrieve.c $(INC)/HTUtils.h $(INC)/tcp.h \
	$(INC)/HTFile.h
	$(CC) -c $(CFLAGS) -DRULES HTRetrieve.c

# 		FIND gateway - test only (has to run on VM!)

FINDGate : $(FOBJS)
	$(CC) -o FINDGate $(FOBJS) $(LFLAGS)
FINDGate.o : $(SRC)/FINDGate.c $(INC)/HTUtils.h $(INC)/tcp.h
	$(CC) -c $(CFLAGS) $(SRC)/FINDGate.c

# 		WAIS Gateway

WAISGate : $(WOBJS) $(WAISLIB)
	$(CC) -o WAISGate $(WOBJS) $(WAISLIB) $(LFLAGS)
WAISGate.o : $(SRC)/WAISGate.c $(INC)/HTUtils.h $(INC)/tcp.h
	$(CC) -c $(CFLAGS) -I$(WIR) $(SRC)/WAISGate.c
wsrc2html.o : $(SRC)/wsrc2html.c $(INC)/HTUtils.h
	$(CC) -c $(CFLAGS) $(SRC)/wsrc2html.c
wsrc2html : wsrc2html.o 
	$(CC) -o wsrc2html wsrc2html.o $(CFLAGS)
	
ParseWSRC.o : $(SRC)/ParseWSRC.c $(INC)/HTUtils.h
	$(CC) -c -o ParseWSRC.o $(CFLAGS) $(SRC)/ParseWSRC.c

#	Stand-alone WAIS source file parser:

ParseWSRC_Prog.o : $(SRC)/ParseWSRC.c $(INC)/HTUtils.h
	$(CC) -c -o ParseWSRC_Prog.o $(CFLAGS) -DTEST $(SRC)/ParseWSRC.c
ParseWSRC : ParseWSRC_Prog.o 
	$(CC) -o ParseWSRC ParseWSRC_Prog.o $(CFLAGS)

#	Test program for WAIS gateway
	
tw : $(TWOBJS) $(WBIN)/ui.a
	$(CC) -o tw $(TWOBJS) $(WBIN)/ui.a -lNeXT_s -lsys_s

# 		Common Code

test.o : $(SRC)/test.c $(INC)/HTUtils.h $(INC)/tcp.h $(INC)/HTTCP.h
	$(CC) -c $(CFLAGS) -DRULES $(SRC)/test.c
HTDaemon.o : $(SRC)/HTDaemon.c $(INC)/HTUtils.h $(INC)/tcp.h $(INC)/HTTCP.h
	$(CC) -c $(CFLAGS) -DRULES $(SRC)/HTDaemon.c
HTDaemonNR.o : $(SRC)/HTDaemon.c $(INC)/HTUtils.h $(INC)/tcp.h $(INC)/HTTCP.h
	$(CC) -c $(CFLAGS) -o HTDaemonNR.o $(SRC)/HTDaemon.c
HTTCP.o : $(CMN)/HTTCP.c $(INC)/tcp.h $(INC)/HTUtils.h
	$(CC) -c $(CFLAGS) $(CMN)/HTTCP.c
HTString.o : $(CMN)/HTString.c $(INC)/tcp.h $(INC)/HTUtils.h
	$(CC) -c $(CFLAGS) $(CMN)/HTString.c
HTFTP.o : $(CMN)/HTFTP.c $(INC)/tcp.h $(INC)/HTUtils.h
	$(CC) -c $(CFLAGS) $(CMN)/HTFTP.c
HTFile.o : $(CMN)/HTFile.c $(INC)/tcp.h $(INC)/HTUtils.h
	$(CC) -c $(CFLAGS) $(CMN)/HTFile.c
HTRules.o : $(SRC)/HTRules.c $(INC)/HTUtils.h $(INC)/HTParse.h
	$(CC) -c $(CFLAGS) -DRULES $(SRC)/HTRules.c
HTParse.o : $(CMN)/HTParse.c $(INC)/HTUtils.h $(INC)/HTParse.h
	$(CC) -c $(CFLAGS) $(CMN)/HTParse.c
HTOracle.o : $(SRC)/HTOracle.c