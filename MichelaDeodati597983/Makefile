CC				=	gcc
ARRVS			=	ar rvs
CFLAG			=	-std=c99 -Wall -Werror -g 
DFLAG			=	-c -o
INCLUDES		=	-I .
LDFLAGS			=	-L .
LIBS			=	-lpthread
LIBS2 			=	$(LDFLAGS) -lCollector -lWorkerPool
LIBSMT 			=	$(LDFLAGS) -lCollector -lWorkerPool $(LIBS)
LIBSMAIN		=	$(LIBS2) -lMasterThread $(LIBS)

TARGETS         =   farm

.PHONY: all clean cleanall
.SUFFIXES: .c .h

all:				$(TARGETS)

farm:				libMasterThread.a main.c 
	$(CC) $(CFLAG) $(INCLUDES) main.c -o farm $(LIBSMAIN)

libMasterThread.a:	MasterThread.o
	$(ARRVS) libMasterThread.a MasterThread.o Collector.o WorkerPool.o

MasterThread.o: 	libWorkerPool.a libCollector.a MasterThread.c
	$(CC) $(CFLAG) $(INCLUDES) MasterThread.c $(DFLAG) MasterThread.o $(LIBSMT)

libWorkerPool.a:	WorkerPool.o
	$(ARRVS) libWorkerPool.a WorkerPool.o

WorkerPool.o:		WorkerPool.c
	$(CC) $(CFLAG) $(INCLUDES) WorkerPool.c $(DFLAG) WorkerPool.o $(LIBS)

libCollector.a:		Collector.o
	$(ARRVS) libCollector.a Collector.o

Collector.o:		Collector.c
	$(CC) $(CFLAG) $(INCLUDES) Collector.c $(DFLAG) Collector.o $(LIBS)

clean:				
	rm -f $(TARGETS)
cleanall:	 		clean
	\rm -f *.o *~ *.a *.dat ./testdir

