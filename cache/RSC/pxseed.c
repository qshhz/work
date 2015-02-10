#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <RSC.h>

int main(int argc, char **argv) {
	/*
	 * Search mtab for a pxfs entry. This implies pxadmin can be used
	 * only if at least one pfs is mounted, which is reasonable.
	 */
	if (argc > 1)
	{

//		char* path = argv[1];
//
//		int fd = open(path, O_RDWR, NULL);
//		if (fd < 0) {
//			fprintf(stderr, "Open of %s failed errno %d\n", path, errno);
//			(void) close(fd);
//			return errno;
//		}
//		if (ioctl(fd, PX_IOCTL_SEEDRSC) == -1) {
//			fprintf(stderr, "PX_IOCTL_SEEDRSC on %s failed errno %d (%s)\n",
//					path, errno, strerror(errno));
//			(void) close(fd);
//			return errno;
//		}
//		fprintf(stdout, "PX_IOCTL_SEEDRSC on %s successful\n", path);
//		(void) close(fd);
//


		char* path = argv[1];

		int fd = open(path, O_RDWR, NULL);
		if (fd < 0) {
			fprintf(stderr, "Open of %s failed errno %d\n", path, errno);
			(void) close(fd);
			return errno;
		}

		cm_cache_io_t t;
		t.i = 11;
		t.j = 12;

		if (ioctl(fd, PX_IOCTL_UPDATE_CACHE, &t) == -1) {
			fprintf(stdout, "PX_IOCTL_SEEDRSC on %s error \n", path);
			close(fd);
		}

		fprintf(stdout, "t.i is %d, and t.j is %d\n", t.i, t.j);
		(void) close(fd);

	}

	return 0;
}
