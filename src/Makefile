#--config

CC = mpicc
CPP = mpic++
CFLAGS += -fPIC -I../..
CPPFLAGS += -fPIC
LDFLAGS +=


#--- building

PROGS = fscull retention

all: $(PROGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

fscull: fscull.o
	$(CPP) $(LDFLAGS) $< -lfsmr -ldftw -lmrmpi -o $@

retention: retention.o
	$(CPP) $(LDFLAGS) $< -lfsmr -ldftw -lmrmpi -o $@


#--- testing

test:
	$(error There are no tests here.  See ../tests/)


#--- cleaning

clean:
	rm -f $(PROG) *.o *.so
