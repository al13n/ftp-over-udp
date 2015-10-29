include ../Make.defines

PROGS =	server.prog
OBJS = server.o rtt.o

all: ${PROGS}

%.prog: ${OBJS}
	${CC} ${FLAGS} -o $@ ${OBJS} ${LIBS}

%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

.PHONY: clean

clean: 
	rm -f *.o *.prog