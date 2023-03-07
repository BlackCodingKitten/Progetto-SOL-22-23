CC				=	gcc
ARRVS			=	ar rvs
CFLAG			+=	-std=c99 -Wall -g
INCLUDES		=	-I.
LDFLAGS			=	-L.
LIBS			=	-lpthread

TARGETS         =   farm

.PHONY: all clean cleanall
.SUFFIXES: .c .h

all:				$(TARGETS)

farm:				main.c libMasterthread.a
libMasterthread.a:	MasterThread.o
MasterThread.o: 	libWorkerPool.a libCollector.a MasterThread.c
libWorkerpool.a:	WorkerPool.o
WorkerPool.o:		WorkerPool.c
libCollector.a:		Collector.o
Collector.o:		Collector.c


