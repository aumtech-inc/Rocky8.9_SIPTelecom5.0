#----------------------------------------------------------------------------
# Program:      Makefile
#
# Purpose:      Make file for building the all ARC DB Services components.
#
# Author:   	Sandeep Agate
#
# Date:    	2000/05/17 sja	Created the file
# Update:    	2000/05/19 sja	Added compiling of Common
#----------------------------------------------------------------------------
include Make.def

all: Always
	( cd ${ML_HOME}/apisrc; 	make all )
#	( cd ${ML}/app; 	make all )

clean: Always
	( cd ${ML_HOME}/apisrc; 	make clean )
#	( cd ${ML}/app; 	make clean )

install: all

Always:
