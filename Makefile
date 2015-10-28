include ../Make.defines

PROGS =	server.prog
OBJS = server.o 


all: ${PROGS}

%.prog: %.o
	${CC} ${FLAGS} -o $@ $< ${LIBS}

%.o: %.cpp
	${CC} ${CFLAGS} -c $< -o $@

.PHONY: clean

clean: 
	rm -f *.o *.prog