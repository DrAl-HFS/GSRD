
CC      = pgcc
CCFLAGS = -c11 -Minfo=all
ACCFLAGS= -DACC -g -Mautoinline -acc=verystrict -ta=multicore,tesla
TARGET= mdev
RUNFLAGS= 0
OBJEXT = o

# Other environments, compilers, options...
UNAME := $(shell uname -a)

ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
TARGET = $(TARGET).exe
OBJEXT = obj
endif

ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC      = clang
CCFLAGS = -std=c11 -W
ACCFLAGS=
endif


all: build run verify

build: $(TARGET).c
	$(CC) $(CCFLAGS) $(ACCFLAGS) -o $(TARGET) $<

run: $(TARGET)
	./$(TARGET) $(RUNFLAGS)

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) *.$(OBJEXT) *.dwf *.pdb prof
