# GSRD makefile

# PGI C compiler is default
CC      = pgcc
CCFLAGS = -c11 -Minfo=all
ABFLAGS = -DACC
# PGI Acceleration options - NB: Build for GTX1080 requires "cc60" to launch on GPU...
#ACCFLAGS = -mp -fast -acc=verystrict -ta=multicore,tesla:cc60
# -Mlarge_arrays # >2GB
# Whereas INKCAP (GTX970M - "cc50") works with default...
#ACCFLAGS = -mp -fast -acc=verystrict -ta=multicore,tesla
#ACCFLAGS = -fast -acc=verystrict -ta=multicore,tesla
ACCFLAGS = -g -Mautoinline -acc=verystrict -ta=host,multicore,tesla
#FAST = -O4 -Mautoinline -acc=verystrict
#OPT = -ta=tesla:managed #ERR: malloc: call to cuMemAllocManaged returned error 3: Not initialized

TARGET = gsrd
RUNOPT = -A:G
RUNFLAGS = -I=100,100 -O:raw/
DATAFILE = "init/gsrd00000(1024,1024,2)F64.raw"
CMP_FILE = "ref/gsrd00100(1024,1024,2)F64.raw"
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

# (Not working...) Fall back to standard LINUX tools if necessary
#ifneq ($(findstring "pgcc", $(CCOUT)), "pgcc")
ifeq ($(CCOUT), "not found")
#@echo $(CCOUT)
CC       = gcc
CCFLAGS  = -std=c11 -W
ACCFLAGS = -fopenacc -fopenmp
ABFLAGS  = -DACC -DOMP
endif

ifeq ($(findstring Debian, $(UNAME)), Debian)
CC       = gcc
CCFLAGS  = -std=c11 -W
ACCFLAGS = -fopenacc -fopenmp
endif

ifeq ($(findstring RPi, $(UNAME)), RPi)
CC       = gcc
CCFLAGS  = -std=c11 -W
DATAFILE = "init/gsrd00000(256,256,2)F64.raw"
CMP_FILE =
#"ref/gsrd00100(256,256,2)F64.raw"
ACCFLAGS = -fopenmp
ABFLAGS  = -DOMP
RUNOPT   = -A:N
endif

# Other environments, compilers, options...
ifeq ($(findstring Darwin, $(UNAME)), Darwin)
CC = clang
CCFLAGS  = -std=c11 -W
DATAFILE = "init/gsrd00000(512,512,2)F64.raw"
CMP_FILE = "ref/gsrd00100(512,512,2)F64.raw"
ABFLAGS  =
RUNOPT   = -A:N
endif

ifeq ($(findstring CYGWIN_NT, $(UNAME)), CYGWIN_NT)
OBJEXT = obj
TARGET = $(TARGET).exe
endif


# top level targets
all:	build run
acc:	buildacc runacc

SRC_DIR=src
OBJ_DIR=obj

#SL = $(shell ls $(SRC_DIR))
SL= gsrd.c proc.c data.c args.c util.c randMar.c image.c
SRC:= $(SL:%.c=$(SRC_DIR)/%.c)
OBJ:= $(SL=:%.c=$(OBJ_DIR)/%.o)

# Default build - no acceleration
build: $(SRC)
	$(CC) $(CCFLAGS) -o $(TARGET)X $(SRC)

buildacc: $(SRC)
	$(CC) $(CCFLAGS) $(ACCFLAGS) $(ABFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)X
	./$(TARGET)X -V=0.1 $(DATAFILE) -C:$(CMP_FILE) $(RUNFLAGS)

runacc: $(TARGET)
	./$(TARGET) $(DATAFILE) -C:$(CMP_FILE) $(RUNFLAGS) $(RUNOPT)
# ./$(TARGET) $(RUNFLAGS) $(RUNOPT)
# 2>> log.txt

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
