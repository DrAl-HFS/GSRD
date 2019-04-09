# GSRD makefile

# PGI C compiler is default
CC      = pgcc
CCFLAGS = -c11
ABFLAGS = -DACC
# PGI Acceleration options - NB: Build for GTX1080 requires "cc60" to launch on GPU...
#ACCFLAGS = -mp -fast -acc=verystrict -ta=multicore,tesla:cc60
ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla
# -Minfo=all
#-mp
#FAST = -O2 -Mautoinline -acc=verystrict

TARGET = gsrd
RUNOPT = -A:A
RUNFLAGS = -M -B:R -I=6000,100,-10,10 -O:dat/raw/ -X:dat/rgb/ -L:"dat/init/viridis-table-byte-0256.csv"
DATAFILE = "dat/init/gsrd00000(1024,1024,2)F64.raw"
CMP_FILE = "dat/ref/gsrd00100(1024,1024,2)F64.raw"

# top level targets
.PHONY: all acc
all:	build run
acc:	buildacc runacc

# Default build - no acceleration
build: ACCFLAGS= 
build: ABFLAGS= 
build: $(TARGET)

buildacc: $(TARGET)

# old builds...
#	$(CC) $(CCFLAGS) -o $(TARGET)X $(SRC)
#	$(CC) $(CCFLAGS) $(ACCFLAGS) $(ABFLAGS) -o $(TARGET) $(SRC)

SRC_DIR=src
HDR_DIR=$(SRC_DIR)
OBJ_DIR=obj

#SL= gsrd.c proc.c data.c args.c util.c randMar.c image.c
#SRC:= $(SL:%.c=$(SRC_DIR)/%.c)
SRC = $(shell ls $(SRC_DIR)/*.c)
OBJ:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)


### Minimal rebuild rules ###

%.o: $(SRC_DIR)/%.c $(HDR_DIR)/%.h
	$(CC) $(CCFLAGS) $(ACCFLAGS) $(ABFLAGS) $< -c

$(OBJ_DIR)/%.o : %.o
	mv $< $@
 
$(TARGET): $(OBJ)
	$(CC) $(CCFLAGS) $(ACCFLAGS) $(LDFLAGS) $^ -o $@


### Special purpose targets ###
.PHONY: tun runacc verify clean

run: $(TARGET)X
	./$(TARGET)X -V=0.1 $(DATAFILE) -C:$(CMP_FILE) $(RUNFLAGS)

runacc: $(TARGET)
	./$(TARGET) $(DATAFILE) -C:$(CMP_FILE) $(RUNFLAGS) $(RUNOPT)

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
