fscull -- distributed filesystem data retention policy enforcement


## To install it:

Make sure you've installed [fsmr](https://github.com/jabrcx/fsmr) and its dependencies ([libcircle](https://github.com/hpc/libcircle), [libdftw](https://github.com/hpc/libdftw), [MR-MPI](http://mapreduce.sandia.gov/), OpenMPI, etc.).
(At FASRC, `module load gcc openmpi fsmr dummy_lsf_libs`.)

Install it in some location `$PREFIX`:

``` bash
wget --no-check-certificate https://github.com/fasrc/fscull/raw/master/fscull-0.0.0b.tar.gz
tar xvf fscull-*.tar.gz
cd fscull-*
./configure --prefix="$PREFIX"
make
make install

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
You may also want to remove one -v , to only print out files which are culled.

See `man fscull` for more info.


## To iteratively code and run the tests:

``` bash
$ git clone git@github.com:/fasrc/fscull.git
$ cd fscull/tests/
$ export PATH=$PWD/../src:$PATH
$ export MANPATH=$PWD/../share/man:$MANPATH
$ alias doit='cd ../src && make -f Makefile.non_autoconf && cd ../tests && make'

$ module load gcc openmpi fsmr dummy_lsf_libs  #(at FASRC)

$ doit
```
