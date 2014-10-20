#suggestion: alias doit='cd ../src && make && cd ../tests && make'


#the directory to create and cull; single directory, relative path
DATA_ROOT = testdata

#the trash directory
TRASH_ROOT = trash

#parallelism -- the number of mpi processes
NPROCS = 3

#other mpi args (e.g. don't bother with IB)
EXTRA_MPIRUN_ARGS = --mca btl self,tcp


all: check create cull clean

check:
	./env_check.sh

create:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT)

cull:
	mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) fscull $(DATA_ROOT) $(TRASH_ROOT)

clean:
	./remove_testdata.sh $(DATA_ROOT) $(TRASH_ROOT)
