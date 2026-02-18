fscull -- distributed filesystem data retention policy enforcement


## About

fscull walks a filesystem and moves files older than a specified age to a trash bin.
It's a distributed application using MPI (really just a wrapper around rename(2), which is then used as the map in [fsmr](https://github.com/jabrcx/fsmr) mapreduce).
It's meant to be deployed on HPC storage such as Lustre, where it can use 100s of processes to scan and move 100s of millions of files on a scale of hours.


## To install it:

fscull depends on an MPI implementation (tested with Open-MPI).

Additionally, fscull has a build-time dependency on several third-party libraries, including:
* [fsmr](https://github.com/jabrcx/fsmr)
* [libcircle](https://github.com/hpc/libcircle)
* [libdftw](https://github.com/hpc/libdftw)
* [MR-MPI](https://github.com/sandialabs/mapreduce)

These have been as git submodules to the ./vendor directory in the git repository.

To download fscull, clone the repository, ensuring the `--recurse-submodules` is specified to fetch the vendored repositories:

``` bash
git clone --recurse-submodules git@github.com:/fasrc/fscull.git
cd fscull
``` bash

Build and install it in some location `$PREFIX`:

``` bash
module load gcc/13.2.0-fasrc01 openmpi/5.0.2-fasrc01 #(at FASRC)
make
make install PREFIX="$PREFIX"
```

And setup your environment to find it:

``` bash
export PATH="$PREFIX/bin:$PATH"
export MANPATH="$PREFIX/share/man:$MANPATH"
```


## To use it:

To do a dry-run and print out whether each file will be culled or not under a 90-day retention policy:

``` bash
mpirun [MPI_OPTIONS] fscull -vv \
     --data-root /PATH/TO/THE/DATA/DIRECTORY \
     --trash-root /PATH/TO/THE/TRASH/DIRECTORY \
     --retention-window $(( 60 * 60 * 24 * 90)) \
     --pretend
```

To run it for real, remove `--pretend`.
To add directories that are exempt from the policy, use `--exempt`.
Use the MPI option `--output-filename` to send each rank's output to a separate file (otherwise you won't be able to grep out the filenames that were affected since output lines will be split and interleaved).
You may also want to remove one -v , to only print out files which are culled.

See `man fscull` for more info.


## To run the tests:

After building, start an interactive allocation with at least 3 tasks, then run the tests:

``` bash
salloc -p test -t 10 -n 3 --mem=4g
module load gcc/13.2.0-fasrc01 openmpi/5.0.2-fasrc01 #(at FASRC)
make test
```
