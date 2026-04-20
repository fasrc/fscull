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

MPIRUN_ENV = OMPI_ALLOW_RUN_AS_ROOT=1 \
             OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 \
             OMPI_MCA_orte_abort_on_non_zero_status=0 \
             OMPI_MCA_rmaps_base_oversubscribe=1 \
             PRTE_MCA_rmaps_default_mapping_policy=:oversubscribe \
             PRTE_MCA_state_base_error_non_zero_exit=0 
MPI_LAUNCH = env $(MPIRUN_ENV) mpirun -np $(NPROCS) $(EXTRA_MPIRUN_ARGS)

LONGPATH_DATA_ROOT = /tmp/fscull_longpath_test_data_root
LONGPATH_TRASH_ROOT = /tmp/fscull_longpath_test_data_root/_trash
EXTERNAL_TRASH_ROOT = external_trash_root
SYMLINK_REAL_DATA_ROOT = symlink_root_real_data
SYMLINK_REAL_TRASH_ROOT = symlink_root_real_trash
SYMLINK_DATA_ROOT = symlink_root_data
SYMLINK_TRASH_ROOT = symlink_root_trash
NORMAL_LOG = normal_cull.log
SYMLINKROOTS_LOG = symlinkroots_cull.log
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
	@echo running symlink-root mode
	@echo
	$(MAKE) symlinkroots
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

symlinkroots: clean env create_symlinkroots cull_symlinkroots verify_symlinkroots
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

create_symlinkroots:
	./create_testdata.sh $(SYMLINK_REAL_DATA_ROOT) $(SYMLINK_REAL_TRASH_ROOT) $(RETENTION_WINDOW)
	ln -s $(SYMLINK_REAL_DATA_ROOT) $(SYMLINK_DATA_ROOT)
	ln -s $(SYMLINK_REAL_TRASH_ROOT) $(SYMLINK_TRASH_ROOT)

cull:
	@rm -f $(NORMAL_LOG)
	@bash -o pipefail -c 'LC_ALL=C $(MPI_LAUNCH) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS) 2>&1 | tee "$(NORMAL_LOG)"'

cull_longpath:
	$(MPI_LAUNCH) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS)

# Open MPI may report success here when ranks finish MPI teardown before
# returning a local non-zero status. The verification targets below assert the
# observable failure behavior instead of relying on the launcher exit code.
cull_collision:
	@rm -f $(COLLISION_LOG)
	@status=0; \
	bash -o pipefail -c 'LC_ALL=C $(MPI_LAUNCH) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS) 2>&1 | tee "$(COLLISION_LOG)"' || status=$$?; \
	printf 'collision mpirun exit status: %s\n' "$$status"; \
	test -f "$(COLLISION_LOG)"

cull_safety:
	@rm -f $(SAFETY_LOG)
	@status=0; \
	bash -o pipefail -c 'LC_ALL=C $(MPI_LAUNCH) \
		fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS) 2>&1 | tee "$(SAFETY_LOG)"' || status=$$?; \
	printf 'safety mpirun exit status: %s\n' "$$status"; \
	test -f "$(SAFETY_LOG)"

cull_symlinkroots:
	@rm -f $(SYMLINKROOTS_LOG)
	@bash -o pipefail -c 'LC_ALL=C $(MPI_LAUNCH) \
		fscull --data-root $(SYMLINK_DATA_ROOT) --trash-root $(SYMLINK_TRASH_ROOT) \
		--retention-window $(RETENTION_WINDOW) \
		`find $(SYMLINK_REAL_DATA_ROOT) -name exempt | xargs -n 1 echo --exempt-path` \
		$(OPTS) 2>&1 | tee "$(SYMLINKROOTS_LOG)"'

verify:
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME)
	test `find $(DATA_ROOT) -wholename $(TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq 0
	test ! -e $(DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -e $(TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(DATA_ROOT)/foo.symlink
	test ! -e $(TRASH_ROOT)/foo.symlink
	test `find $(TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(TRASH_ROOT) -name '*.deleteme' | wc -l` -eq $(NUM_DELETEME)
	! grep -q '/_trash/' $(NORMAL_LOG)

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

verify_symlinkroots:
	test -L $(SYMLINK_DATA_ROOT)
	test -L $(SYMLINK_TRASH_ROOT)
	test `find $(SYMLINK_REAL_DATA_ROOT) -wholename $(SYMLINK_REAL_TRASH_ROOT) -prune -o -name '*.keepme'   -print | wc -l` -eq $(NUM_KEEPME)
	test `find $(SYMLINK_REAL_DATA_ROOT) -wholename $(SYMLINK_REAL_TRASH_ROOT) -prune -o -name '*.deleteme' -print | wc -l` -eq 0
	test ! -e $(SYMLINK_REAL_DATA_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -e $(SYMLINK_REAL_TRASH_ROOT)/exempt-sibling/prefix_bug.deleteme
	test -L $(SYMLINK_REAL_DATA_ROOT)/foo.symlink
	test ! -e $(SYMLINK_REAL_TRASH_ROOT)/foo.symlink
	test `find $(SYMLINK_REAL_TRASH_ROOT) -name '*.keepme'   | wc -l` -eq 0
	test `find $(SYMLINK_REAL_TRASH_ROOT) -name '*.deleteme' | wc -l` -eq $(NUM_DELETEME)
	! grep -q '/symlink_root_real_trash/' $(SYMLINKROOTS_LOG)

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
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) --retention-window 0 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected zero --retention-window to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) --retention-window -5 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected negative --retention-window to fail' >&2; \
		exit 1; \
	fi; \
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) --retention-window 99999999999999999999 >/dev/null 2>&1; then \
		echo '*** ERROR *** expected overflowing --retention-window to fail' >&2; \
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
	OUTSIDE_EXEMPT=$$(mktemp -d /tmp/fscull_outside_exempt.XXXXXX); \
	if fscull --data-root $(DATA_ROOT) --trash-root $(TRASH_ROOT) --retention-window 1 --exempt-path "$$OUTSIDE_EXEMPT" >/dev/null 2>&1; then \
		echo '*** ERROR *** expected out-of-tree --exempt-path to fail' >&2; \
		rm -rf -- "$$OUTSIDE_EXEMPT"; \
		exit 1; \
	fi; \
	rm -rf -- "$$OUTSIDE_EXEMPT"; \
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
	./remove_testdata.sh $(SYMLINK_REAL_DATA_ROOT) $(SYMLINK_REAL_TRASH_ROOT)
	rm -f $(SYMLINK_DATA_ROOT)
	rm -f $(SYMLINK_TRASH_ROOT)
	rm -f $(NORMAL_LOG)
	rm -f $(SYMLINKROOTS_LOG)
	rm -f $(COLLISION_LOG)
	rm -f $(SAFETY_LOG)

clean_longpath:
	./remove_testdata.sh $(LONGPATH_DATA_ROOT) $(LONGPATH_TRASH_ROOT)
	rm -f $(NORMAL_LOG)
	rm -f $(SYMLINKROOTS_LOG)
	rm -f $(COLLISION_LOG)
	rm -f $(SAFETY_LOG)
