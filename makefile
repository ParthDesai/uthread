CC=gcc
LINKER=ar
SOURCES=src/uthread.c
LIBRARY=libuthread
LIBDIR=bin

debug: 
	mkdir -p $(LIBDIR)
	$(CC) -c -g -lrt $(SOURCES) -o $(LIBDIR)/$(LIBRARY).o
	ar -rsv $(LIBDIR)/$(LIBRARY).a $(LIBDIR)/$(LIBRARY).o


release: 
	mkdir -p $(LIBDIR)
	$(CC) -c -lrt -O2 $(SOURCES) -o $(LIBDIR)/$(LIBRARY).o
	ar -rsv $(LIBDIR)/$(LIBRARY).a $(LIBDIR)/$(LIBRARY).o

clean: 
	rm $(LIBDIR)/$(LIBRARY).o $(LIBDIR)/$(LIBRARY).a       
