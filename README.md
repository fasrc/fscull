fscull -- distributed filesystem data retention policy enforcement

``` bash
$ git clone git@github.com:/fasrc/fscull.git
$ cd fscull/tests/
$ module load gcc openmpi fsmr dummy_lsf_libs
$ export PATH=$PWD/../src:$PATH
$ alias doit='cd ../src && make && cd ../tests && make'
$ doit
```
