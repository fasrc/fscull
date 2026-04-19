#-- config

CC = mpicc
CXX = mpic++
AR ?= ar
CFLAGS += -fPIC
CPPFLAGS +=
LDFLAGS +=
SUBMAKE = $(MAKE) MAKEFLAGS=

ifneq ($(shell command -v $(CXX) >/dev/null 2>&1; echo $$?),0)
CXX = mpicc
endif

VENDOR_DIR := $(abspath ../vendor)
VENDOR_PREFIX ?= $(VENDOR_DIR)/.local

LIBCIRCLE_DIR := $(VENDOR_DIR)/libcircle
LIBDFTW_DIR := $(VENDOR_DIR)/libdftw
FSMR_DIR := $(VENDOR_DIR)/fsmr
MRMPI_VENDOR_DIR := $(VENDOR_DIR)/mapreduce/src

# Optional prefix and flags for MR-MPI.
MRMPI_PREFIX ?=
MRMPI_INCLUDE ?=
MRMPI_LIB ?=
MRMPI_MACHINE ?= mpicc
MRMPI_STATIC ?=
MRMPI_CXX ?= $(CXX)
MRMPI_DEP :=

ifneq ($(strip $(MRMPI_PREFIX)),)
MRMPI_INCLUDE += -I$(MRMPI_PREFIX)/include
MRMPI_LIB += -L$(MRMPI_PREFIX)/lib
ifeq ($(strip $(MRMPI_STATIC)),)
MRMPI_STATIC := $(MRMPI_PREFIX)/lib/libmrmpi.a
endif
MRMPI_DEP := $(MRMPI_STATIC)
else ifneq ($(wildcard $(MRMPI_VENDOR_DIR)/cmapreduce.h),)
MRMPI_INCLUDE += -I$(MRMPI_VENDOR_DIR)
MRMPI_LIB += -L$(MRMPI_VENDOR_DIR)
MRMPI_DEP := $(MRMPI_VENDOR_DIR)/libmrmpi_$(MRMPI_MACHINE).a
endif

VENDOR_INCLUDE := -I$(VENDOR_PREFIX)/include -I$(FSMR_DIR)
VENDOR_LIBPATH := -L$(FSMR_DIR) -L$(VENDOR_PREFIX)/lib
VENDOR_RPATH := -Wl,-rpath,$(FSMR_DIR) -Wl,-rpath,$(VENDOR_PREFIX)/lib

CPPFLAGS += $(VENDOR_INCLUDE) $(MRMPI_INCLUDE)
LDFLAGS += $(VENDOR_LIBPATH) $(MRMPI_LIB) $(VENDOR_RPATH)

LIBCIRCLE_A := $(VENDOR_PREFIX)/lib/libcircle.a
LIBDFTW_A := $(VENDOR_PREFIX)/lib/libdftw.a
FSMR_A := $(FSMR_DIR)/libfsmr.a
LIBCIRCLE_BOOTSTRAP := $(LIBCIRCLE_DIR)/.bootstrap.stamp
LIBDFTW_BOOTSTRAP := $(LIBDFTW_DIR)/.bootstrap.stamp

FSMR_CFLAGS := $(CFLAGS) $(VENDOR_INCLUDE) $(MRMPI_INCLUDE)
FSMR_CPPFLAGS := $(VENDOR_INCLUDE) $(MRMPI_INCLUDE) $(VENDOR_LIBPATH) $(MRMPI_LIB) $(VENDOR_RPATH)
LIBDFTW_CPPFLAGS := -I$(VENDOR_PREFIX)/include -include stddef.h
LIBDFTW_LDFLAGS := -L$(VENDOR_PREFIX)/lib
# MR-MPI built from C++ may require legacy MPI C++ binding symbols at link time.
# Set MPI_CXX_LIBS=-lmpi_cxx for older MPI implementations
MPI_CXX_LIBS ?= 
STATIC_LIBS ?= $(MPI_CXX_LIBS) -lstdc++ -lm

PROGS = fscull
LEGACY_PROGS = retention

.PHONY: all clean


#--- building

all: $(PROGS)

$(PROGS): $(FSMR_A) $(LIBDFTW_A) $(LIBCIRCLE_A) $(MRMPI_DEP)

%.o: %.c $(FSMR_A)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

fscull: fscull.o
	$(CC) $(LDFLAGS) $< $(FSMR_A) $(LIBDFTW_A) $(LIBCIRCLE_A) $(MRMPI_DEP) $(STATIC_LIBS) -o $@

$(FSMR_A): $(LIBDFTW_A) $(MRMPI_DEP) $(FSMR_DIR)/Makefile $(FSMR_DIR)/fsmr.c $(FSMR_DIR)/fsmr.h
	$(MAKE) -C $(FSMR_DIR) \
		MAKEFLAGS= \
		CC="$(CC)" CPP="$(CXX)" \
		CFLAGS="$(FSMR_CFLAGS)" \
		CPPFLAGS="$(FSMR_CPPFLAGS)" \
		fsmr.o
	rm -f $@
	$(AR) rcs $@ $(FSMR_DIR)/fsmr.o

$(MRMPI_VENDOR_DIR)/libmrmpi_$(MRMPI_MACHINE).a: $(MRMPI_VENDOR_DIR)/Makefile $(MRMPI_VENDOR_DIR)/MAKE/Makefile.$(MRMPI_MACHINE)
	$(SUBMAKE) -C $(MRMPI_VENDOR_DIR) $(MRMPI_MACHINE) CC="$(MRMPI_CXX)"
ifneq ($(strip $(MRMPI_DEP)),)
ifneq ($(MRMPI_DEP),$(MRMPI_VENDOR_DIR)/libmrmpi_$(MRMPI_MACHINE).a)
$(MRMPI_DEP):
	@test -f $@
endif
endif

$(LIBDFTW_A): $(LIBCIRCLE_A) $(LIBDFTW_BOOTSTRAP) $(LIBDFTW_DIR)/Makefile.am $(LIBDFTW_DIR)/configure.ac $(LIBDFTW_DIR)/libdftw/dftw.c
	cd $(LIBDFTW_DIR) && \
		PKG_CONFIG_PATH="$(VENDOR_PREFIX)/lib/pkgconfig:$${PKG_CONFIG_PATH:-}" \
		./configure --prefix="$(VENDOR_PREFIX)" \
			CC="$(CC)" \
			CPPFLAGS="$(LIBDFTW_CPPFLAGS)" \
			LDFLAGS="$(LIBDFTW_LDFLAGS)" \
			--enable-static --disable-shared
	$(SUBMAKE) -C $(LIBDFTW_DIR) \
		CPPFLAGS="$(LIBDFTW_CPPFLAGS)" \
		LDFLAGS="$(LIBDFTW_LDFLAGS)"
	$(SUBMAKE) -C $(LIBDFTW_DIR) install \
		CPPFLAGS="$(LIBDFTW_CPPFLAGS)" \
		LDFLAGS="$(LIBDFTW_LDFLAGS)"

$(LIBCIRCLE_A): $(LIBCIRCLE_BOOTSTRAP) $(LIBCIRCLE_DIR)/Makefile.am $(LIBCIRCLE_DIR)/configure.ac $(LIBCIRCLE_DIR)/libcircle/lib.c
	cd $(LIBCIRCLE_DIR) && ./configure --prefix="$(VENDOR_PREFIX)" CC="$(CC)" --enable-static --disable-shared
	$(SUBMAKE) -C $(LIBCIRCLE_DIR)
	$(SUBMAKE) -C $(LIBCIRCLE_DIR) install

$(LIBCIRCLE_BOOTSTRAP): $(LIBCIRCLE_DIR)/configure.ac $(LIBCIRCLE_DIR)/Makefile.am
	cd $(LIBCIRCLE_DIR) && ./autogen.sh
	touch $@

$(LIBDFTW_BOOTSTRAP): $(LIBDFTW_DIR)/configure.ac $(LIBDFTW_DIR)/Makefile.am
	cd $(LIBDFTW_DIR) && ./autogen.sh
	touch $@

#--- cleaning

clean:
	rm -f $(PROGS) $(LEGACY_PROGS) *.o $(LIBCIRCLE_BOOTSTRAP) $(LIBDFTW_BOOTSTRAP)
