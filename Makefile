# GSRD makefile

# PGI C compiler
CC= pgcc -c11
AOPT= -O4
NOPT= -O2 -DDEBUG
LOPT= 
ABFLAGS= 
# PGI Acceleration options - NB: Build for GTX1080 requires "cc60" to launch on GPU...
#FAST= -O2 -Mautoinline -acc=verystrict
#ACCFLAGS= -mp -fast -acc=verystrict -ta=multicore,tesla:cc60
ACCFLAGS= -Mautoinline -acc=verystrict -ta=host,multicore,tesla
LDFLAGS= -acc

TARGET= gsrd
RUNOPT= -A:A
RUNFLAGS= -M -B:R -I=1000,100,0,0 -O:dat/raw/ -X:dat/rgb/ -L:"dat/init/viridis-table-byte-0256.csv"
DATAFILE= "dat/init/gsrd00000(1024,1024,2)F64.raw"
CMP_FILE= "dat/ref/gsrd00100(1024,1024,2)F64.raw"

# top level targets
.PHONY: acc all dbg
acc: $(TARGET)
all: clean run
dbg: AOPT= -g -Minfo=all
dbg: NOPT= -g -Minfo=all
dbg: LOPT= -g

# Unaccelerated build/run
nacc: $(TARGET)
nacc: ACCFLAGS= 
nacc: ABFLAGS= -DNACC -DNOMP
nacc: RUNOPT= -A:N

# Source partitioned into sub directories according to acc options
SRC_DIR=src
HDR_DIR=$(SRC_DIR)
SRCA_DIR=src/acc
HDRA_DIR=$(SRCA_DIR)
OBJ_DIR=obj

BLD_DIR:= $(shell pwd)
INCLUDES:= -I$(BLD_DIR)/$(SRC_DIR) -I$(BLD_DIR)/$(SRCA_DIR)

SRC:= $(shell ls $(SRC_DIR)/*.c)
OBJ:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
SRCA:= $(shell ls $(SRCA_DIR)/*.c)
OBJA:= $(SRCA:$(SRCA_DIR)/%.c=$(OBJ_DIR)/%.o)


### Minimal rebuild rules ###

%.o: $(SRCA_DIR)/%.c $(HDRA_DIR)/%.h
	$(CC) $(AOPT) $(ACCFLAGS) $(INCLUDES) $(ABFLAGS) $< -c

%.o: $(SRC_DIR)/%.c $(HDR_DIR)/%.h
	$(CC) $(NOPT) $(INCLUDES) $(ABFLAGS) $< -c

$(OBJ_DIR)/%.o : %.o
	mv $< $@
 
$(TARGET): $(OBJ) $(OBJA)
	$(CC) $(LOPT) $(LDFLAGS) $^ -o $@


### Special purpose targets ###
.PHONY: verbose run verify clean

verbose: $(SRC) $(SRCA)
	@echo SRC=$(SRC) SRCA=$(SRCA)

run: $(TARGET)
	./$(TARGET) $(DATAFILE) -C:$(CMP_FILE) $(RUNFLAGS) $(RUNOPT)

dbg: $(TARGET)
	./$(TARGET) $(DATAFILE) -M -B:R -I=100,10,0,0 -A:G

verify:


clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ_DIR)/*.o *.dwf *.pdb prof
