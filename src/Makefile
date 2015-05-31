# -*- mode: makefile -*-
# 
# Makefile for compiling 'mpl3115a2d' on Raspberry Pi. 
#
# Wed Dec 10 21:16:03 CET 2014
# Edit: Wed May 27 22:41:31 CEST 2015
# Jaakko Koivuniemi

CXX           = gcc
CXXFLAGS      = -g -O -Wall 
LD            = gcc
LDFLAGS       = -lm -O

%.o : %.c
	$(CXX) $(CXXFLAGS) -c $<

all: mpl3115a2d 

mpl3115a2d: I2cWriteRegister.o mpl3115a2d.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

