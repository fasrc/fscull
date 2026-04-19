#suggestion if making iterative changes within source tree:
#    alias doit='cd ../src && make -f Makefile.non_autoconf && cd ../tests && make'


#the directory to create and cull -- be careful, data here will be deleted!
DATA_ROOT = test_data_root

#the trash directory
TRASH_ROOT = test_data_root/_trash

#the retention window in seconds
RETENTION_WINDOW = 86400

#other opts
OPTS = -vv

#parallelism -- the number of mpi processes
NPROCS = 3

#other mpi args
EXTRA_MPIRUN_ARGS = --mca btl self,tcp
#EXTRA_MPIRUN_ARGS = $(EXTRA_MPIRUN_ARGS) --output-filename out --tag-output

LONGPATH_DATA_ROOT = /tmp/fscull_longpath_test_data_root
LONGPATH_TRASH_ROOT = /tmp/fscull_longpath_test_data_root/_trash
EXTERNAL_TRASH_ROOT = external_trash_root
COLLISION_LOG = collision_cull.log
SAFETY_LOG = safety_fail.log

#expected results
NUM_TOTAL = 22
NUM_KEEPME = 13
NUM_DELETEME = 11
NUM_KEEPME_LONGPATH = 14
NUM_DELETEME_LONGPATH = 12
NUM_KEEPME_COLLISION = 14
NUM_DELETEME_COLLISION = 11
NUM_DELETEME_UNCULLED_COLLISION = 1
NUM_DELETEME_SAFETY = 12
NUM_DELETEME_UNCULLED_SAFETY = 2

# This suite does not currently exercise the FTW_DNR/FTW_NS traversal error
# branches in fscull.c. In this root-running devcontainer they are difficult to
# trigger reliably without a separate non-root harness or explicit fault
# injection.


all:
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running in pretend mode
	@echo
	$(MAKE) pretend
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running in normal mode
	@echo
	$(MAKE) normal
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running long-path mode
	@echo
	$(MAKE) longpath
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running external-trash mode
	@echo
	$(MAKE) externaltrash
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running collision-failure mode
	@echo
	$(MAKE) collision
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running symlink/no-overwrite failure mode
	@echo
	$(MAKE) safetyfail
	@echo
	@echo
	@echo -------------------------------------------------------------------------------
	@echo running argument-failure mode
	@echo
	$(MAKE) argfail
	@echo
	@echo SUCCESS


normal: env create cull verify clean

pretend: OPTS += -p
pretend: env create cull verify_pretend clean

longpath: DATA_ROOT=$(LONGPATH_DATA_ROOT)
longpath: TRASH_ROOT=$(LONGPATH_TRASH_ROOT)
longpath: clean_longpath env create_longpath cull_longpath verify_longpath
	$(MAKE) clean_longpath

externaltrash: TRASH_ROOT=$(EXTERNAL_TRASH_ROOT)
externaltrash: clean env create cull verify
	$(MAKE) clean TRASH_ROOT="$(TRASH_ROOT)"

collision: clean env create_collision cull_collision verify_collision
	$(MAKE) clean

safetyfail: clean env create_safety cull_safety verify_safety
	$(MAKE) clean

argfail: clean env create argcheck
	$(MAKE) clean


env:
	./env_check.sh

create:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT) $(RETENTION_WINDOW)

create_longpath:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT) $(RETENTION_WINDOW) --with-long-path

create_collision:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT) $(RETENTION_WINDOW) --with-trash-collision

create_safety:
	./create_testdata.sh $(DATA_ROOT) $(TRASH_ROOT) $(RETENTION_WINDOW) --with-safety-failures

cull:
	mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS)

cull_longpath:
	mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS)

cull_collision:
	@rm -f $(COLLISION_LOG)
	@set -e; \
	if LC_ALL=C mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS) >$(COLLISION_LOG) 2>&1; then \
		echo '*** ERROR *** expected collision-mode cull to fail' >&2; \
		cat $(COLLISION_LOG) >&2; \
		exit 1; \
	fi

cull_safety:
	@rm -f $(SAFETY_LOG)
	@set -e; \
	if LC_ALL=C mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS) >$(SAFETY_LOG) 2>&1; then \
		echo '*** ERROR *** expected symlink/no-overwrite mode to fail' >&2; \
		cat $(SAFETY_LOG) >&2; \
		exit 1; \
	fi

verify:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME)
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq 0
	test ! -e $(DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -e $(TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(DATA_ROOT)/foo.symlink
	test ! -e $(TRASH_ROOT)/foo.symlink
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -eq $(NUM_DELETEME)

verify_pretend:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME)
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq $(NUM_DELETEME)
	test -e $(DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test ! -e $(TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(DATA_ROOT)/foo.symlink
	test ! -e $(TRASH_ROOT)/foo.symlink
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -eq 0

verify_longpath:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME_LONGPATH)
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq 0
	test ! -e $(DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -e $(TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(DATA_ROOT)/foo.symlink
	test ! -e $(TRASH_ROOT)/foo.symlink
	test `find $(DATA_ROOT)/long_path_repro -name 'long_path.deleteme' | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -eq $(NUM_DELETEME_LONGPATH)

verify_collision:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME_COLLISION)
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq $(NUM_DELETEME_UNCULLED_COLLISION)
	test ! -e $(DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -e $(TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(DATA_ROOT)/foo.symlink
	test ! -e $(TRASH_ROOT)/foo.symlink
	test -e $(DATA_ROOT)/trash_collision_repro/child/collision_bug.deleteme
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -eq $(NUM_DELETEME_COLLISION)
	grep -qi 'not a directory' $(COLLISION_LOG)

verify_safety:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME)
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq $(NUM_DELETEME_UNCULLED_SAFETY)
	test ! -e $(DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -e $(TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(DATA_ROOT)/foo.symlink
	test ! -e $(TRASH_ROOT)/foo.symlink
	test -e $(DATA_ROOT)/leaf_collision_repro/leaf_collision.deleteme
	grep -qx 'source' $(DATA_ROOT)/leaf_collision_repro/leaf_collision.deleteme
	grep -qx 'trash' $(TRASH_ROOT)/leaf_collision_repro/leaf_collision.deleteme
	test -e $(DATA_ROOT)/symlink_trap_repro/trapped.deleteme
	test ! -e $(TRASH_ROOT)/symlink_trap_target/trapped.deleteme
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -eq $(NUM_DELETEME_SAFETY)
	grep -qi 'file exists' $(SAFETY_LOG)
	grep -qi 'too many levels of symbolic links' $(SAFETY_LOG)

argcheck:
	@set -e; \
	if fscull >/dev/null 2>&1; then \
		echo '*** ERROR *** expected missing-arguments invocation to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected missing --trash-root to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --trash-root $(TRASH_ROOT) --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected missing --data-root to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) --retention-window nope >/dev/null 2>&1; then \
		echo '*** ERROR *** expected invalid --retention-window to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT)-missing --trash-root $(TRASH_ROOT) --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected missing data root to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT)-missing --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected missing trash root to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) --retention-window 1 --exempt-path $(DATA_ROOT)/missing >/dev/null 2>&1; then \
		echo '*** ERROR *** expected missing --exempt-path to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT)/foo.keepme --trash-root $(TRASH_ROOT) --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected non-directory data root to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --trash-root $(DATA_ROOT)/bar.keepme --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected non-directory trash root to fail' >&2; \
		exit 1; \
	fi; \
	DATA_DEV=$$(stat -c %d $(DATA_ROOT)); \
	CROSSFS_ROOT=''; \
	for CANDIDATE in /dev/shm /var/tmp /tmp; do \
		if [ -d "$$CANDIDATE" ]; then \
			CANDIDATE_DEV=$$(stat -c %d "$$CANDIDATE"); \
			if [ "$$CANDIDATE_DEV" != "$$DATA_DEV" ]; then \
				CROSSFS_ROOT="$$CANDIDATE/fscull_crossfs_trash.$$PPID"; \
				break; \
			fi; \
		fi; \
	done; \
	if [ -z "$$CROSSFS_ROOT" ]; then \
		echo '*** ERROR *** unable to find an alternate filesystem for testing' >&2; \
		exit 1; \
	fi; \
	mkdir -p "$$CROSSFS_ROOT"; \
	if fscull --data-root $(DATA_ROOT) --trash-root "$$CROSSFS_ROOT" --retention-window 1 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected cross-filesystem trash root to fail' >&2; \
		rm -rf -- "$$CROSSFS_ROOT"; \
		exit 1; \
	fi; \
	rm -rf -- "$$CROSSFS_ROOT"

clean:
	./remove_testdata.sh $(DATA_ROOT) $(TRASH_ROOT)
	rm -f $(COLLISION_LOG)
	rm -f $(SAFETY_LOG)

clean_longpath:
	./remove_testdata.sh $(LONGPATH_DATA_ROOT) $(LONGPATH_TRASH_ROOT)
	rm -f $(COLLISION_LOG)
	rm -f $(SAFETY_LOG)
