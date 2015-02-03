#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <RSC.h>

static int create_cmfile_with_data(off_t ncblk, off_t nfile, PX_BOOL r_f, PX_BOOL hdonly_f)
{
	if (!r_f && 0 == access(CACHE_FILE, F_OK))
	{
		fprintf(stderr,
				"read side cache file %s already exists, use -r to recreate cache file.\n",
				CACHE_FILE);
		return EINVAL;
	}
	PX_ASSERT(ncblk > 0);
	PX_ASSERT(nfile > 0);

//	FILE *cache_fp = fopen(CACHE_FILE, "w+b");
	int cache_fp;
	if (hdonly_f)
	{
		cache_fp = open(CACHE_FILE, O_CREAT | O_WRONLY);
	}
	else
	{
		cache_fp = open(CACHE_FILE, O_CREAT | O_WRONLY | O_TRUNC);
	}

	if (cache_fp == 0)
	{
		(void) fprintf(stderr, "error %d opening cache file %s\n", errno,
		CACHE_FILE);
		return errno;
	}

	char* buf = (char*) malloc(RSCBLKSIZE);
//	char buf[RSCBLKSIZE];
	off_t i;

	/*------- write master block -----------*/
	MasterBlock mb;

	g_file_header_section = mb.file_header_section = mb.file_header_flist =
			sizeof(MasterBlock);
	mb.file_header_flist_tail = sizeof(MasterBlock)
			+ sizeof(FileHeader) * (nfile - 1);
	g_block_pointer_section = mb.block_pointer_section =
			mb.block_pointer_flist = sizeof(MasterBlock)
					+ sizeof(FileHeader) * nfile;
	mb.block_pointer_flist_tail = sizeof(MasterBlock)
			+ sizeof(FileHeader) * nfile
			+ sizeof(BlockPointerNode) * (ncblk - 1);
	g_block_section = mb.block_section = mb.block_pointer_flist_tail
			+ sizeof(BlockPointerNode);
	g_disk_capacity = mb.capacity = mb.block_section + (RSCBLKSIZE) * ncblk;
	mb.cblksize = RSCBLKSIZE;

	size_t nwritten = write(cache_fp, &mb, sizeof(MasterBlock));
	if (nwritten != sizeof(MasterBlock))
	{
		char msg[MAX_MSG];
		sprintf(msg, "error MasterBlock, %s\n", strerror(errno));
		RSCLOG(msg, FATAL_F);

		return errno;
	}

	off_t fn_offset = mb.block_pointer_flist;
	off_t total_off = 0;

	/*------- write file name ---------- */
	i = 0;

	FileHeader fh;
	while (i < nfile)
	{
		fh.loc = mb.file_header_flist + i * sizeof(FileHeader);
		if (i < nfile - 1)
			fh.next = mb.file_header_flist + (i + 1) * sizeof(FileHeader);
		else
			fh.next = INVLOC; // last file header

		fh.nodeHeader = INVLOC;
		fh.nodeTail = INVLOC;
		fh.fileoff = INVLOC;
//		bzero(fh.fi.path, MAX_MFS_PATH);

		size_t nwritten = write(cache_fp, &fh, sizeof(FileHeader));
		if (nwritten != sizeof(FileHeader))
		{
			char msg[MAX_MSG];
			sprintf(msg, "error FileHeader, %s\n", strerror(errno));
			RSCLOG(msg, FATAL_F);

			return errno;
		}
		i++;
	}
	printf("file name section finished!\n");

	/*------- write block pointer ---------- */
	i = 0;
	total_off += fn_offset;
	off_t memoff = total_off + ncblk * sizeof(BlockPointerNode);
	BlockPointerNode bn;
	while (i < ncblk)
	{
		bn.loc = total_off + i * sizeof(BlockPointerNode);
		if (i < ncblk - 1)
			bn.next = total_off + (i + 1) * sizeof(BlockPointerNode);
		else
			bn.next = INVLOC; // last file header
		bn.memloc = memoff + i * RSCBLKSIZE;
		bn.fhloc = INVLOC;
		bn.fileblockoff = INVLOC;

		size_t nwritten = write(cache_fp, &bn, sizeof(BlockPointerNode));
		if (nwritten != sizeof(BlockPointerNode))
		{
			char msg[MAX_MSG];
			sprintf(msg, "error %ld write cache file offset %ld len %ld, %s\n",
					nwritten, (size_t) (total_off + sizeof(FileHeader) * i),
					sizeof(FileHeader), strerror(errno));
			RSCLOG(msg, FATAL_F);

			return errno;
		}

		i++;
	}
	printf("block pointer section finished!\n");

	if (hdonly_f)
	{
		PX_ASSERT(close(cache_fp)==0);

		cache_fp = open(CACHE_FILE, O_RDONLY);
		struct stat fileStat;
		PX_ASSERT(fstat(cache_fp, &fileStat) >= 0);

		size_t filesize = lseek(cache_fp, 0l, SEEK_END);
		PX_ASSERT(filesize == fileStat.st_size);
		RSCLOG("RSC cache is reset!", NORMAL_F);
		return 0;
	}

	/*------- write block info ---------- */
	i = 0;
	bzero(buf, RSCBLKSIZE);
	total_off += ncblk * sizeof(BlockPointerNode);

	while (i < ncblk)
	{
		if (i % 10 == 0)
		{
			printf("%.2f%%", i * 100.0 / ncblk);
			fflush(stdout);
			printf("\r");
		}

		size_t nwritten = write(cache_fp, buf, RSCBLKSIZE);
		if (nwritten != RSCBLKSIZE)
		{
			char msg[MAX_MSG];
			sprintf(msg, "error %ld write cache file offset %ld len %ld, %s\n",
					nwritten, (size_t) (total_off + sizeof(FileHeader) * i),
					sizeof(FileHeader), strerror(errno));
			RSCLOG(msg, FATAL_F);
			return errno;
		}
		i++;
	}
	printf("block section finished!\n");

	free(buf);

	struct stat fileStat;
	PX_ASSERT(fstat(cache_fp, &fileStat) >= 0);

	PX_ASSERT(close(cache_fp)==0);

//	printf("%s total_off %ld, RSCBLKSIZE * i %ld, fileStat.st_size %ld / %ld", CACHE_FILE, total_off, RSCBLKSIZE * i, fileStat.st_size, total_off+ RSCBLKSIZE * i);
	PX_ASSERT(total_off + RSCBLKSIZE * i == fileStat.st_size);
	PX_ASSERT(g_disk_capacity == fileStat.st_size);

	RSCLOG("RSC cache is reset!", NORMAL_F);

	return 0;
}

#if 0
static int read_cmfile_with_data(off_t ncblk, off_t nfile)
{

	FILE *cache_fp = fopen(CACHE_FILE, "r");

	if (cache_fp == 0)
	{
		(void) fprintf(stderr, "error %d opening cache file %s\n", errno,
				CACHE_FILE);
		return errno;
	}

	off_t total_off = 0;
	off_t i;

	/*------- read master block -----------*/
	MasterBlock mb;

	size_t nread = fread(&mb, sizeof(MasterBlock), 1, cache_fp);
	if (nread != 1)
	{
		fprintf(stderr, "read master block error!\n");
		return errno;
	}
	else
	{
		printf("file_header_flist %ld, block_pointer_flist %ld\n\n",
				mb.file_header_flist, mb.block_pointer_flist);
	}

	/*------- read file name ---------- */
	i = 0;
	FileHeader fh;
	while (i < nfile)
	{
		size_t nread = fread(&fh, sizeof(FileHeader), 1, cache_fp);
		if (nread != 1)
		{
			fprintf(stderr, "error %ld read cache file offset %ld len %ld\n",
					nread, (size_t) (total_off + sizeof(FileHeader) * i),
					sizeof(FileHeader));
			return errno;
		}
		else
		{
			printf("FileHeader loc %ld, next %ld, nodeHeader %ld\n", fh.loc,
					fh.next, fh.nodeHeader);
		}
		i++;
	}

	i = 0;
	BlockPointerNode bn =
	{	INVLOC, INVLOC, INVLOC, INVLOC};
	total_off = sizeof(MasterBlock) + nfile * sizeof(FileHeader); // + sizeof(BlockPointerNode)*ncblk;
//    printf("\n%ld \n", total_off);
	while (i < ncblk)
	{
//        printf("total_off %ld \n", total_off);
		fseek(cache_fp, total_off, SEEK_SET);
		size_t nread = fread(&bn, sizeof(BlockPointerNode), 1, cache_fp);
		if (nread != 1)
		{
			fprintf(stderr, "error %ld reading cache file offset %ld len %ld\n",
					nread, (size_t) (total_off + sizeof(BlockPointerNode) * i),
					sizeof(BlockPointerNode));
			return errno;
		}
		else
		{
			char buff[RSCBLKSIZE];
			fseek(cache_fp, bn.memloc, SEEK_SET);
			size_t nread = fread(buff, RSCBLKSIZE, 1, cache_fp);
			if (nread == 1)
			printf("loc %ld, next %ld, memloc %ld, str %s\n", bn.loc,
					bn.next, bn.memloc, buff);
			else
			printf("error!!!!\n");
		}
		total_off += sizeof(BlockPointerNode);
		i++;
	}

	fseek(cache_fp, 0L, SEEK_END);
	off_t filesize = ftell(cache_fp);
	assert(bn.memloc + RSCBLKSIZE == filesize);
	fclose(cache_fp);

	return 0;
}
#endif

int main(int argc, char **argv)
{
	PX_BOOL r_f = PX_FALSE;
	PX_BOOL hdonly_f = PX_FALSE;
	if (argc == 2)
	{
		if (strcmp(argv[1], "-r") == 0)
		{
			r_f = PX_TRUE;
		}
		else if (strcmp(argv[1], "-rh") == 0)
		{
			r_f = PX_TRUE;
			hdonly_f = PX_TRUE;
		}
	}

	size_t size;
	readrscconf(CACHE_FILE, g_logfile, &size, &RSCBLKSIZE);
	PX_ASSERT(strlen(CACHE_FILE));
	PX_ASSERT(strlen(g_logfile));
	PX_ASSERT(isexp2(RSCBLKSIZE));
	printf("path-%s\n", CACHE_FILE);
	printf("log-%s\n", g_logfile);
	printf("size-%ldG\n", size);
	printf("cblksize-%ld\n", RSCBLKSIZE);
	g_log_fp = fopen(g_logfile, "a+");

	off_t rsc_size = size * 1024 * 1024 * 1024;
	PX_ASSERT(rsc_size % RSCBLKSIZE == 0);
	off_t ncblk = rsc_size / RSCBLKSIZE;
	off_t nfiles = ncblk / 10;
	if (nfiles < MINNUMFILES)
		nfiles = MINNUMFILES;
	printf("The num of file headers is %ld\n", nfiles);
	printf("The num of cache blocks is %ld\n", ncblk);

	create_cmfile_with_data(ncblk, nfiles, r_f, hdonly_f);
	return 0;
}

