#--config

CC = mpicc
CPP = mpic++
CFLAGS += -fPIC -I../..
CPPFLAGS += -fPIC
LDFLAGS += -L../..


#--- building

PROG = fscull

all: $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(PROG).o
	$(CPP) $(LDFLAGS) $< -lfsmr -ldftw -lmrmpi -o $@


#--- testing

define TEST_INSTRUCTIONS
endef
export TEST_INSTRUCTIONS

test_args:
ifndef TEST_ARGS
		$(warning `make test' requires the TEST_ARGS environment variable be set.)
		$(warning Set it to the path of a directory to purge, for example:)
		$(warning export TEST_ARGS='/tmp')
		$(error You must set the TEST_ARGS environment variable.)
endif

test: $(PROG) test_args
	@echo "$$TEST_INSTRUCTIONS"
	LD_LIBRARY_PATH=../..:"$$LD_LIBRARY_PATH" mpirun -np 3 --mca btl self,tcp ./$(PROG) $(TEST_ARGS)


#--- cleaning

clean:
	rm -f $(PROG) *.o *.so
