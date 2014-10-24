fscull -- distributed filesystem data retention policy enforcement


## To install it:

Install it in some location `$PREFIX`:

``` bash
wget https://github.com/fasrc/fscull/blob/master/fscull-0.0.0-a.tar.gz
tar xvf fscull-*.tar.gz
cd fscull-*
./configure --prefix="$PREFIX"
make
make install

export PATH="$PREFIX/bin:$PATH"
export MANPATH="$PREFIX/man:$MANPATH"
```


## To use it:

To do a dry-run and print out whether each file will be culled or not under a 90-day retention policy:

``` bash
fscull -vv \
     --data-root /PATH/TO/THE/DATA/DIRECTORY \
     --trash-root /PATH/TO/THE/TRASH/DIRECTORY \
     --retention-window $(( 60 * 60 * 24 * 90)) \
     --pretend
```

To run it for real, remove `--pretend`.
You may also want to remove one -v , to only print out files which are culled.
Run do a dry run:

``` bash
fscull \
	--data-root PATH_TO_PURGE \
	--trash-root TRASH_DIRECTORY \
	--retention-window SECONDS \
	-vv \
	-p
```

To run it for real, remove `-p`.
You may also want to remove one `-v`, to only print out culled files.


## To iteratively code and run the tests (at FASRC):

``` bash
$ git clone git@github.com:/fasrc/fscull.git
$ cd fscull/tests/
$ module load gcc openmpi fsmr dummy_lsf_libs
$ export PATH=$PWD/../src:$PATH
$ export MANPATH=$PWD/../man:$MANPATH
$ alias doit='cd ../src && make -f Makefile.non_autoconf && cd ../tests && make'

$ doit
```
