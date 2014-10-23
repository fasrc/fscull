#suggestion if making iterative changes within source tree:
#    alias doit='cd ../src && make -f Makefile.non_autoconf && cd ../tests && make'


#the directory to create and cull -- be careful, data here will be deleted!
DATA_ROOT = test_data_root

#the trash directory
TRASH_ROOT = test_data_root/_trash

#the retention window in seconds
RETENTION_WINDOW = 86400

#parallelism -- the number of mpi processes
NPROCS = 3

#other mpi args
EXTRA_MPIRUN_ARGS = --mca btl self,tcp
#EXTRA_MPIRUN_ARGS = $(EXTRA_MPIRUN_ARGS) --output-filename out --tag-output


all: env create cull verify clean

env:
	./env_check.sh

create:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT) $(RETENTION_WINDOW)

cull:
	mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path`

verify:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -gt 0
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -gt 0

clean:
	./remove_testdata.sh $(DATA_ROOT) $(TRASH_ROOT)
