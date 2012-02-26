#		Make WorldWideWeb daemons   - sun4
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

#CFLAGS = -Wall -I$(INC) -DDEBUG -g
CFLAGS =  -I$(INC) -DDEBUG -g
LFLAGS =
CC = cc

include $(SRC)/CommonMakefile
