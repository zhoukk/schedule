.PHONY : mingw linux undefined

CFLAGS = -g -Wall -I/usr/local/include
LDFLAGS := -lpthread
SHARED := -shared

SRC = index.c queue.c schedule.c lschedule.c serial.c

UNAME=$(shell uname)
SYS=$(if $(filter Linux%,$(UNAME)),linux,\
	$(if $(filter MINGW%,$(UNAME)),mingw,\
	$(if $(filter Darwin%,$(UNAME)),macosx,\
	undefined\
	)))

all: $(SYS)

undefined:
	@echo "I can't guess your platform, please do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "      linux mingw macosx"

mingw : TARGET := schedule.dll
mingw : CFLAGS += 
mingw : LDFLAGS += -L/usr/local/bin -llua53
mingw : SHARED += 
mingw : schedule

linux : TARGET := schedule.so
linux : CFLAGS +=
linux : LDFLAGS += -Wl,-rpath,/usr/local/lib -llua
linux : SHARED += -fPIC
linux : schedule

schedule : $(SRC)
	gcc $(CFLAGS) -o $(TARGET) $^ $(SHARED) $(LDFLAGS)

clean :
	-rm -f schedule.dll
	-rm -f schedule.so
