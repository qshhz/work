debug = 1

vpath %.c progs

includes = -I. -IRSC -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
 

CC = gcc
CFLAGS := $(includes) -fPIC -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -Wall -Wno-format-security -pthread
LDLIBS = -lpthread -lrt -lglib-2.0 `pkg-config fuse --libs` -lglib-2.0 `pkg-config --cflags glib-2.0`

ifeq ($(debug), 1)
CFLAGS := -ggdb3 $(CFLAGS)
else
CFLAGS := -DNDEBUG $(CFLAGS)
LDFLAGS := -s 
endif

rsc_objects = RSC/util.o RSC/RSC.o RSC/RSC_m.o RSC/prefetch.o
#wbt_objects = thd/wbt.o
main_objects = cachec.o 
mkrcf_objects = RSC/mkrcf.o 
rscck_objects = RSC/rscck.o 
objects = $(rsc_objects) $(mkrcf_objects) $(rscck_objects) $(main_objects)
# $(wbt_objects)

progs = cache mkrcf rscck

.PHONY: all
all: $(libs) $(progs) $(objects)

	
	
cache: $(main_objects) $(rsc_objects)
	$(CC) -o $@ $(LDFLAGS) $^  $(LDLIBS)
	
rscck: $(rscck_objects) $(rsc_objects)
	$(CC) -o $@ $(LDFLAGS) $^  $(LDLIBS)
	
mkrcf: $(mkrcf_objects) $(rsc_objects)
	$(CC) -o $@ $(LDFLAGS) $^  $(LDLIBS)

.PHONY: progs
progs: $(progs)

#.PHONY: install
#install:
#	install $(installopts) $(libs) $(prefix)/lib/
#	install $(installopts) $(progs) $(prefix)/bin/

.PHONY: distclean
distclean: clean
	rm -f $(libs) $(progs)

.PHONY: clean
clean:
	rm -f $(objects) $(progs)

# vi: set noexpandtab sw=4 ts=4:
