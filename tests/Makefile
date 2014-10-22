#suggestion: alias doit='cd ../src && make && cd ../tests && make'


#the directory to create and cull; single directory, relative path
DATA_ROOT = testdata

#the trash directory
TRASH_ROOT = trash

#parallelism -- the number of mpi processes
NPROCS = 3

#other mpi args (e.g. don't bother with IB)
EXTRA_MPIRUN_ARGS = --mca btl self,tcp
#EXTRA_MPIRUN_ARGS = $(EXTRA_MPIRUN_ARGS) --output-filename out --tag-output


all: env create cull verify clean

env:
	./env_check.sh

create:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT)

cull:
	mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) fscull $(DATA_ROOT) $(TRASH_ROOT)

verify:
	test `find $(DATA_ROOT)  -name '*.deleteme' | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0

clean:
	./remove_testdata.sh $(DATA_ROOT) $(TRASH_ROOT)
