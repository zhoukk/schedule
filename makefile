SRC = index.c queue.c schedule.c lschedule.c serial.c

all : schedule.dll

schedule.dll : $(SRC)
	gcc -g -Wall -I/usr/local/include --shared -o$@ $^ -lpthread -L/usr/local/bin -llua53

clean :
	rm -f schedule.dll
