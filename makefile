#!/bin/bash

CC=g++
LINKER=g++
HEADERS=secureSocketStream.h
CPPFLAGS=-D_FILE_OFFSET_BITS=64
LINKFLAGS=-lcrypto -lssl
ODIR=inter
BINDIR=bin

all: $(BINDIR)/xfer

$(BINDIR)/xfer: $(BINDIR)/.exists $(ODIR)/secureSocketStream.o $(ODIR)/main.o
	$(LINKER) $(LINKFLAGS) -o $(BINDIR)/xfer $(ODIR)/secureSocketStream.o $(ODIR)/main.o

$(ODIR)/.exists:
	mkdir -p $(ODIR)
	touch $(ODIR)/.exists

$(BINDIR)/.exists:
	mkdir -p $(BINDIR)
	touch $(BINDIR)/.exists
	
$(ODIR)/main.o: main.cpp $(HEADERS) $(ODIR)/.exists 
	mkdir -p $(ODIR)
	$(CC) $(CPPFLAGS) -c -o $(ODIR)/main.o main.cpp

$(ODIR)/secureSocketStream.o: secureSocketStream.cpp $(HEADERS) $(ODIR)/.exists 
	$(CC) $(CPPFLAGS) -c -o $(ODIR)/secureSocketStream.o secureSocketStream.cpp

clean:
	rm -f $(ODIR)/*.o
	rm -f $(BINDIR)/xfer
