#
# Copyright (c) 2013, John A. Brunelle
# All rights reserved.
#


#--config

CC = mpicc
CPP = mpic++
CFLAGS += -fPIC -I../..
CPPFLAGS += -fPIC
LDFLAGS += -L../..


#--- building

PROG = retention

all: $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(PROG).o
	$(CPP) $(LDFLAGS) $< -lfsmr -ldftw -lmrmpi -o $@


#--- testing

define TEST_INSTRUCTIONS

double check total size against:
	find $(TEST_ARGS) -type f | xargs stat -c%s | awk '{sum+=$$1} END {print sum}'

endef
export TEST_INSTRUCTIONS

test_args:
ifndef TEST_ARGS
		$(warning `make test' requires the TEST_ARGS environment variable be set.)
		$(warning Set it to the path of a directory to walk, for example:)
		$(warning export TEST_ARGS='/opt')
		$(error You must set the TEST_ARGS environment variable.)
endif

test: $(PROG) test_args
	@echo "$$TEST_INSTRUCTIONS"
	LD_LIBRARY_PATH=../..:"$$LD_LIBRARY_PATH" mpirun -np 3 --mca btl self,tcp ./$(PROG) $(TEST_ARGS)


#--- cleaning

clean:
	rm -f $(PROG) *.o *.so
