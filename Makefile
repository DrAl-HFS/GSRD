# GSRD makefile

# PGI C compiler is default
CC      = pgcc
CCFLAGS = -c11
# -Minfo=all
ABFLAGS = -DACC
# PGI Acceleration options - NB: Build for GTX1080 requires "cc60" to launch on GPU...
#ACCFLAGS = -mp -fast -acc=verystrict -ta=multicore,tesla:cc60
ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla
#-mp
#FAST = -O2 -Mautoinline -acc=verystrict

TARGET = gsrd
RUNOPT = -A:G
RUNFLAGS = -M -B:R -I=100,100 -O:dat/raw/ -X:dat/rgb/ -L:"dat/init/viridis-table-byte-0256.csv"
DATAFILE = "dat/init/gsrd00000(1024,1024,2)F64.raw"
CMP_FILE = "dat/ref/gsrd00100(1024,1024,2)F64.raw"
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

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

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
