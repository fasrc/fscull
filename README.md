fscull -- distributed filesystem data retention policy enforcement


## About

fscull walks a filesystem and moves files older than a specified age to a trash bin.
It's a distributed application using MPI (really just a wrapper around rename(2), which is then used as the map in [fsmr](https://github.com/jabrcx/fsmr) mapreduce).
It's meant to be deployed on HPC storage such as Lustre, where it can use 100s of processes to scan and move 100s of millions of files on a scale of hours.


## To install it:

The repo provides a standard GNU-toolchain-style tarball for building.

Make sure you've installed [fsmr](https://github.com/jabrcx/fsmr) and its dependencies ([libcircle](https://github.com/hpc/libcircle), [libdftw](https://github.com/hpc/libdftw), [MR-MPI](http://mapreduce.sandia.gov/), [OpenMPI](http://www.open-mpi.org/), etc.).
(At FASRC, `module load gcc openmpi fsmr dummy_lsf_libs`.)

Download it:

``` bash
wget --no-check-certificate https://github.com/fasrc/fscull/raw/master/fscull-0.0.0b.tar.gz
tar xvf fscull-*.tar.gz
cd fscull-*
```

Install it in some location `$PREFIX`:

``` bash
./configure --prefix="$PREFIX"
make
make install
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
Use the MPI option `--output-filename` to send each rank's output to a separate file (otherwise you won't be able to grep out the filenames that were affected from the interleaved output).
You may also want to remove one -v , to only print out files which are culled.

See `man fscull` for more info.


## To iteratively code and run the tests:

Get setup:

``` bash
$ git clone git@github.com:/fasrc/fscull.git
$ cd fscull/tests/
$ export PATH=$PWD/../src:$PATH
$ export MANPATH=$PWD/../share/man:$MANPATH
$ module load gcc openmpi fsmr dummy_lsf_libs  #(at FASRC)
```

Then iteratively make changes to files in ../src/ and run:

``` bash
$ cd ../src && make -f Makefile.non_autoconf && cd ../tests && make
```
