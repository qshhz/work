#include <RSC.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>


int main(int argc, char **argv) {

	g_RSC_table_m = (RSC_table_m*) calloc((size_t) 1, sizeof(RSC_table_m));
	PX_ASSERT(g_RSC_table_m != NULL);
	g_RSC_table_m->tab = g_hash_table_new_full(g_str_hash, g_str_equal, Cleanup,
			(void*) Destory_file_block_table); // Destory_file_block_table will free key space
	PX_ASSERT(g_RSC_table_m->tab != NULL);

	PX_ASSERT(pthread_mutexattr_init(&g_recursive_mt_attr) == 0);
	PX_ASSERT(pthread_mutexattr_settype(&g_recursive_mt_attr, PTHREAD_MUTEX_RECURSIVE) == 0);
	PX_ASSERT(pthread_mutex_init(&g_RSC_table_m->mt, &g_recursive_mt_attr) == 0);
	PX_ASSERT(pthread_mutex_init(&g_RSC_freelist_mt, &g_recursive_mt_attr) == 0);

	readrscconf(CACHE_FILE, g_logfile, NULL, &RSCBLKSIZE);
	if (0 != access(CACHE_FILE, F_OK))
	{
		fprintf(stderr, "read side cache file %s does not exist!\n",
		CACHE_FILE);
		PX_ASSERT(0);
	}

#ifdef USEFREAD
	PX_ASSERT(0 == access(CACHE_FILE, F_OK));
	g_cache_fp = fopen(CACHE_FILE, "r+b");
#else
	PX_ASSERT(0 == access(CACHE_FILE, F_OK));
	g_cache_fp = open(CACHE_FILE, O_RDWR);
#endif
	g_log_fp = fopen(g_logfile, "a+");
	SetGlobalSection();
	ReadWholeDiskToRSCTable();
//	PX_ASSERT(fclose(g_cache_fp) == 0);
//	return 0;
//
//	g_cache_fp = fopen(CACHE_FILE, "r+b");

	while (1) {
		printf("1. print blockheader\n"
				"2. print whole cache\n"
				"3. print blockheader freelist\n"
				"4. print fileheader freelist\n"
				"5. space left\n"
				"0. exit\n");
		int com;
		scanf("%d", &com);
		if (com == 1) {
			printf(" printWholeBlockPointer()\n");
			printWholeBlockPointer();
		} else if (com == 2) {
			printf(" printWholeDisk()\n");
			printWholeDisk();
		} else if (com == 3) {
			printf(" printbnfreelist()\n");
			printbnfreelist();
		} else if (com == 4) {
			printf(" printfileheaderfreelist()\n");
			printfileheaderfreelist();
		} else if (com == 5) {
			printf(" space left\n");
			printspaceleft();
		} else if (com == 0) {
			break;
		} else
			break;
	}

	PX_ASSERT(fclose(g_log_fp) == 0);
	PX_ASSERT(close(g_cache_fp) == 0);

	return 0;
}
