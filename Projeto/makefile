CC	=gcc
FLAGS	=-Wall -pthread
PROGM	=home_iot
PROGS	=sensor
PROGU	=user_console
OBJM	=main.o functions.o
OBJS	=sensor.o functions.o
OBJU	=userconsole.o functions.o
CTARG	=${OBJM} ${OBJS} ${OBJU} ${PROGM} ${PROGS} ${PROGU}


all:	${PROGM} ${PROGS} ${PROGU}

.PHONY: clean
clean:
	rm -f $(wildcard *.o)


${PROGM}:	${OBJM}
	${CC} ${FLAGS} ${OBJM} -lm -o $@

${PROGS}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -lm -o $@

${PROGU}:	${OBJU}
	${CC} ${FLAGS} ${OBJU} -lm -o $@


.c.o:
	${CC} ${FLAGS} $< -c -o $@

#######################################

functions.o:	functions.c header.h

userconsole.o:	userconsole.c header.h

sensor.o:	sensor.c header.h

main.o:		main.c header.h

