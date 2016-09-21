DEBUG = -g -DDEBUG
CC = gcc
CFLAGS  = $(DEBUG) -Wall
LIB	= -I /usr/local/include -L /usr/local/lib -L /usr/local/lib64 	\
	  -lpthread -lrt -lcurl -ljson-c \
	  -Wl,-rpath,/usr/local/lib	\
	  -Wl,-rpath,/usr/local/lib64
OO	= clagent.o					\
	  ca_string.o				\
	  ca_array.o 				\
	  ca_heap.o                 \
	  ca_so.o					\
	  ca_log.o 					\
	  ca_util.o 				\
	  ca_daemon.o 				\
	  ca_threadpool.o           \
	  ca_conf.o					\
	  ca_update.o				\
	  ca_acquisition.o			\
	  ca_worker.o				\
	  acq/ca_cpu.o				\
	  acq/ca_disk_io.o			\
	  acq/ca_disk_urate.o		\
	  acq/ca_load_average.o		\
	  acq/ca_memory.o			\
	  acq/ca_net_flow.o


TARGETS = clagent

all: $(TARGETS)

$(TARGETS): $(OO)
	$(CC) $(CFLAGS) $(OO) -o $@ $(LIBDIR) $(LIB)

.PHONY:

install:
	install $(TARGETS) ../bin/

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@ $(INC) $(MYSQLCFLAGS)

clean:
	rm -f *.o
	rm -f acq/*.o
	rm -f $(TARGETS)
