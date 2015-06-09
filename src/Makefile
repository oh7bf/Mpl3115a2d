# -*- mode: makefile -*-
# 
# Makefile for compiling 'mpl3115a2d' on Raspberry Pi. 
#
# Wed Dec 10 21:16:03 CET 2014
# Edit: Tue Jun  9 21:15:16 CEST 2015
# 
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -lm -lsqlite3 -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: mpl3115a2d 

mpl3115a2d: I2cWriteRegister.o I2cRReadRegBytes.o ReadSQLiteTime.o InsertSQLite.o mpl3115a2d.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

