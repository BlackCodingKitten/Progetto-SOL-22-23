CC				=	gcc
ARRVS			=	ar rvs
CFLAG			+=	-std=c99 -Wall -Werror -g -Wextra -O3
DFLAG			=	-c -o
INCLUDES		=	-I .
LDFLAGS			=	-L .
LIBS			=	-lpthread
LIBS2 			=	$(LDFLAGS) -lCollector -lWorkerPool
LIBSMT 			=	$(LDFLAGS) -lCollector -lWorkerPool $(LIBS)
LIBSMAIN		=	$(LIBS2) -lMasterThread $(LIBS) 			
OBJECT			= 	./Collector.o ./WorkerPool.o ./MasterThread.o
LIBSA		= 	./libCollector.a ./libMasterThread.a ./libWorkerPool.a
FILES		=   file*.dat explected.txt ./testdir
CLEAN		= 	rm -f -r $(OBJECT) $(LIBSA)

TARGETS         =   farm generafile 

.PHONY: all clean cleanall Socket runtest
.SUFFIXES: .c .h

all:				$(TARGETS) 

generafile:		generafile.c
	$(CC) $(CFLAG) $(INCLUDES) generafile.c -o generafile

farm:				libMasterThread.a main.c 
	$(CC) $(CFLAG) $(INCLUDES) main.c -o farm $(LIBSMAIN)
	echo TEST: per eseguire i test usare "make runtest" 

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
	$(CLEAN)

Socket: 
	rm -f -r farm.sck

cleanall: 
	$(CLEAN) ./farm ./generafile $(FILES) 

runtest:
	chmod u+rw test.sh
	./test.sh

