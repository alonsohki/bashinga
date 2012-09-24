PROGRAM=bashinga
OBJS=main.o io.o prompt.o comandos.o infolinea.o comodines.o terminal.o historial.o match.o variables.o aliases.o
CFLAGS=-pipe -Wall -g
CC=gcc

all: ${PROGRAM}

${PROGRAM}: ${OBJS}
	$(CC) $(OBJS) $(LFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o ${PROGRAM}

main.o: main.c config.h Makefile infolinea.h io.h prompt.h comandos.h comodines.h terminal.h variables.h aliases.h
io.o: io.c config.h Makefile io.h codigos_secuencia.h prompt.h
prompt.o: prompt.c config.h Makefile prompt.h
comandos.o: comandos.c config.h Makefile comandos.h historial.h io.h infolinea.h comodines.h variables.h aliases.h
infolinea.o: infolinea.c config.h infolinea.h Makefile
comodines.o: comodines.c comodines.h config.h Makefile io.h match.h infolinea.h terminal.h prompt.h
terminal.o: terminal.c config.h Makefile terminal.h io.h codigos_secuencia.h   vt100.h
historial.o: historial.c config.h Makefile historial.h io.h
match.o: match.c match.h Makefile
variables.o: variables.c variables.h config.h Makefile infolinea.h
aliases.o: aliases.c aliases.h config.h Makefile infolinea.h terminal.h io.h
