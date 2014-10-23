AC_INIT([fscull], [0.0.1-temporary], [rchelp@fas.harvard.edu])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

#fscull itself is C, but the autoconf lib checks need to be C++ due to using mrmpi
AC_LANG_CPLUSPLUS

CC=mpicc
CXX=mpicxx
AC_PROG_CPP

#libdftw
AC_CHECK_HEADER(libdftw.h, [], [
	AC_MSG_ERROR([cannot find libdftw headers -- see https://github.com/hpc/libdftw])
])
AC_CHECK_LIB(dftw, dftw, [], [
	AC_MSG_ERROR([cannot find libdftw library -- see https://github.com/hpc/libdftw])
])

#mrmpi
AC_CHECK_HEADER(cmapreduce.h, [], [
	AC_MSG_ERROR([cannot find MR-MPI headers -- see http://mapreduce.sandia.gov/])
])
AC_CHECK_LIB(mrmpi, MR_kv_add, [], [
	AC_MSG_ERROR([cannot find MR-MPI library -- see http://mapreduce.sandia.gov/])
])

#fsmr
AC_CHECK_HEADER(fsmr.h, [], [
	AC_MSG_ERROR([cannot find fsmr headers -- see https://github.com/jabrcx/fsmr])
])
AC_CHECK_LIB(fsmr, fsmr, [], [
	AC_MSG_ERROR([cannot find fsmr library -- see https://github.com/jabrcx/fsmr])
])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])
AC_OUTPUT
