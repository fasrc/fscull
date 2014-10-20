#--config

CC = mpicc
CPP = mpic++
CFLAGS += -fPIC -I../..
CPPFLAGS += -fPIC
LDFLAGS += 


#--- building

PROG = fscull

all: $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(PROG).o
	$(CPP) $(LDFLAGS) $< -lfsmr -ldftw -lmrmpi -o $@


#--- testing

test:
	$(error There are no tests here.  See ../tests/)


#--- cleaning

clean:
	rm -f $(PROG) *.o *.so
