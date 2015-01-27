# work

#install glib.h lib
sudo apt-get install libglib2.0-dev 
Run the following command, and put the output of the command in your Makefile.
$ pkg-config --cflags --libs glib-2.0

-I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -lglib-2.0

#install fuse

#cmd option: 
/public_CIFS_2  -d -o nonempty -o  big_writes,max_write=131072  -o allow_other


