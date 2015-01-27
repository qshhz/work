#include <RSC.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

void Read_cache_off(void* buf, off_t size, off_t off)
{
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif

	if (off + size > g_disk_capacity)
	{
		char msg[MAX_MSG];
		sprintf(msg, "off+size %ld, g_disk_capacity %ld, errno:%s\n",
				off + size, g_disk_capacity, strerror(errno));
		RSCLOG(msg, FATAL_F);
	}
	PX_ASSERT(off + size <= g_disk_capacity);

	bzero(buf, size);
	size_t nread, expectnread;
	PX_LOCK(&g_RSC_freelist_mt);
#ifdef USEFREAD
	PX_ASSERT(fseek(g_cache_fp, off, SEEK_SET) == 0);
	nread = fread(buf, size, 1, g_cache_fp);
	expectnread = 1;
#else
	nread = pread(g_cache_fp, buf, size, off);
	expectnread = size;
#endif
	PX_UNLOCK(&g_RSC_freelist_mt);
	if (nread != expectnread)
	{
		char msg[MAX_MSG];
		sprintf(msg, "size %ld, off %ld, errno:%s\n", size, off,
				strerror(errno));
		RSCLOG(msg, FATAL_F);
	}
	PX_ASSERT(nread == expectnread);
}

void Write_cache_off(void* buf, off_t size, off_t off)
{
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	PX_ASSERT(off + size <= g_disk_capacity);
	PX_LOCK(&g_RSC_freelist_mt);

	size_t nwritten, expectwritten;
#ifdef USEFREAD
	PX_ASSERT(fseek(g_cache_fp, off, SEEK_SET) == 0);
	nwritten = fwrite(buf, size, 1, g_cache_fp);
	expectwritten = 1;
	PX_ASSERT(fflush(g_cache_fp) == 0);
#else
	nwritten = pwrite(g_cache_fp, buf, size, off);
	expectwritten = size;
#endif
	PX_UNLOCK(&g_RSC_freelist_mt);
	if (nwritten != expectwritten)
	{
		char msg[MAX_MSG];
		sprintf(msg, "size %ld, off %ld, errno:%s\n", size, off,
				strerror(errno));
		RSCLOG(msg, FATAL_F);
	}
	PX_ASSERT(nwritten == expectwritten);
}

// alloc a node from the beginning of the free list
// TODO wait on freelist is NULL, resume when freelist avaiable
void Return_file_block(BlockPointerNode* bn_p)
{
	PX_LOCK(&g_RSC_freelist_mt);

	PX_ASSERT(bn_p != NULL); // TODO remove later
	PX_ASSERT(
			bn_p->loc!=INVLOC && (bn_p->loc - g_block_pointer_section)%sizeof(BlockPointerNode) == 0); // TODO remove later
	PX_ASSERT(BLOCKHEADERINDEX(bn_p->loc) == BLOCKINDEX(bn_p->memloc)); // TODO remove later
	// read master block
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	PX_ASSERT(mb.file_header_section == g_file_header_section); // TODO remove later
	PX_ASSERT(mb.block_pointer_section == g_block_pointer_section); // TODO remove later

	if (mb.block_pointer_flist != INVLOC) // still block available
	{
		BlockPointerNode bn_tail;
		Read_cache_off(&bn_tail, sizeof(BlockPointerNode),
				mb.block_pointer_flist_tail);

		//update current tail block
		bn_tail.next = bn_p->loc;
		Write_cache_off(&bn_tail, sizeof(BlockPointerNode),
				mb.block_pointer_flist_tail);

		//update updated tail block
		mb.block_pointer_flist_tail = bn_p->loc;
		bn_p->next = INVLOC;
		bn_p->fhloc = INVLOC;
		bn_p->fileblockoff = INVLOC;
		Write_cache_off(bn_p, sizeof(BlockPointerNode),
				mb.block_pointer_flist_tail);
	} else // no block available
	{
		PX_ASSERT(mb.block_pointer_flist_tail == INVLOC);
		//update current tail block
		bn_p->next = INVLOC;
		bn_p->fhloc = INVLOC;
		bn_p->fileblockoff = INVLOC;
		Write_cache_off(bn_p, sizeof(BlockPointerNode), bn_p->loc);

		mb.block_pointer_flist = bn_p->loc;
		mb.block_pointer_flist_tail = mb.block_pointer_flist;
	}

	// update master block at the end
	Write_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);

	PX_UNLOCK(&g_RSC_freelist_mt);
}

// alloc a node from the beginning of the free list
// TODO wait on freelist is NULL, resume when freelist avaiable
void Alloc_file_block(BlockPointerNode* bn_p)
{
	PX_LOCK(&g_RSC_freelist_mt);

	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	PX_ASSERT(mb.file_header_section == g_file_header_section);
	PX_ASSERT(mb.block_pointer_section == g_block_pointer_section);

	if (mb.block_pointer_flist != INVLOC) // still block available
	{
		Read_cache_off(bn_p, sizeof(BlockPointerNode), mb.block_pointer_flist); // allocate in here
		PX_ASSERT(
				bn_p->loc!=INVLOC && (bn_p->loc - g_block_pointer_section)%sizeof(BlockPointerNode) == 0);
		mb.block_pointer_flist = bn_p->next;

		if (mb.block_pointer_flist == INVLOC) // current block is the last block on free list
		{
			mb.block_pointer_flist_tail = INVLOC;
		}

		if (BLOCKHEADERINDEX(bn_p->loc) != BLOCKINDEX(bn_p->memloc))
		{
			printf("bn_p->loc %ld, bn_p->memloc %ld\n", (bn_p->loc),
					(bn_p->memloc));
			printf("size %ld, off %ld\n", BLOCKHEADERINDEX(bn_p->loc),
					BLOCKINDEX(bn_p->memloc));
		}
		PX_ASSERT(BLOCKHEADERINDEX(bn_p->loc) == BLOCKINDEX(bn_p->memloc)); // TODO remove later

		// update master block at the end
		Write_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	} else // no block available
	{
		bzero(bn_p, sizeof(BlockPointerNode)); // this will make every member of BlockPointerNode struct to be 0
		bn_p->loc = INVLOC;
	}

	PX_UNLOCK(&g_RSC_freelist_mt);
}

// alloc a node from the beginning of the free list
void Return_file_header(FileHeader* fh_p)
{
	PX_LOCK(&g_RSC_freelist_mt);
	PX_ASSERT(fh_p != NULL);
	PX_ASSERT(
			fh_p->loc!=INVLOC && (fh_p->loc - g_file_header_section)%sizeof(FileHeader) == 0);

	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	PX_ASSERT(mb.file_header_section == g_file_header_section);
	PX_ASSERT(mb.block_pointer_section == g_block_pointer_section);

	if (mb.file_header_flist != INVLOC) // still block available
	{
		FileHeader fh_tail;
		Read_cache_off(&fh_tail, sizeof(FileHeader), mb.file_header_flist_tail);
		PX_ASSERT(fh_p->loc != INVLOC);

		//update current tail header
		fh_tail.next = fh_p->loc;
		Write_cache_off(&fh_tail, sizeof(FileHeader),
				mb.file_header_flist_tail);

		//update updated tail header
		mb.file_header_flist_tail = fh_p->loc;
		fh_p->next = INVLOC;
		fh_p->nodeHeader = INVLOC;
		fh_p->nodeTail = INVLOC;
		fh_p->fileoff = INVLOC;
		bzero(fh_p->path, MAX_MFS_PATH);
		Write_cache_off(fh_p, sizeof(FileHeader), mb.file_header_flist_tail);
	} else // no file header available
	{
		PX_ASSERT(mb.file_header_flist_tail == INVLOC);
		//update current tail block
		fh_p->next = INVLOC;
		fh_p->nodeHeader = INVLOC;
		fh_p->nodeTail = INVLOC;
		fh_p->fileoff = INVLOC;
		Write_cache_off(fh_p, sizeof(FileHeader), fh_p->loc);

		mb.file_header_flist = fh_p->loc;
		mb.file_header_flist_tail = mb.file_header_flist;
	}

	// update master block at the end
	Write_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);

	PX_UNLOCK(&g_RSC_freelist_mt);
}

// alloc a node from the beginning of the free list
// TODO wait on freelist is NULL, resume when freelist avaiable
void Alloc_file_header(FileHeader* fh_p, const char* path)
{

	PX_LOCK(&g_RSC_freelist_mt);
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	PX_ASSERT(mb.file_header_section == g_file_header_section); // TODO remove later
	PX_ASSERT(mb.block_pointer_section == g_block_pointer_section); // TODO remove later

	if (mb.file_header_flist != INVLOC) // still block available
	{
		Read_cache_off(fh_p, sizeof(FileHeader), mb.file_header_flist); // allocate in here
		PX_ASSERT(
				fh_p->loc!=INVLOC && (fh_p->loc - g_file_header_section)%sizeof(FileHeader) == 0);
		strcpy(fh_p->path, path);
		Write_cache_off(fh_p, sizeof(FileHeader), mb.file_header_flist); // write fh into disk
		Read_cache_off(fh_p, sizeof(FileHeader), mb.file_header_flist); // allocate in here
		PX_ASSERT(
				fh_p->loc!=INVLOC && (fh_p->loc - g_file_header_section)%sizeof(FileHeader) == 0);
		PX_ASSERT(strcmp(fh_p->path, path) == 0);

		mb.file_header_flist = fh_p->next;

		if (mb.file_header_flist == INVLOC) // current block is the last block on free list
		{
			mb.file_header_flist_tail = INVLOC;
		}
		Write_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	} else // no file header available
	{
		bzero(fh_p, sizeof(FileHeader)); // this will make every member of BlockPointerNode struct to be INVLOC
		fh_p->loc = INVLOC;
	}
	PX_UNLOCK(&g_RSC_freelist_mt);
}

void printWholeDisk()
{
	printf("\n---------------------disk section----------");
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;

	FileHeader fh;

	off_t i;
	i = mb.file_header_section;
	while (i < mb.block_pointer_section)
	{
		Read_cache_off(&fh, sizeof(FileHeader), i);
		if(fh.nodeHeader != INVLOC)
			printbn_d(&fh);
		i += sizeof(FileHeader);
	}
	printf("---------------------disk section end ----------\n");
}

void printWholeBlockPointer()
{
	printf("\n---------------------BlockPointer section----------\n");
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;

	off_t i = g_block_pointer_section;
	BlockPointerNode bn;
	while (i <= g_block_section - sizeof(BlockPointerNode))
	{
		Read_cache_off(&bn, sizeof(BlockPointerNode), i);
		i += sizeof(BlockPointerNode);
		printbn(&bn);
	}
	printf("---------------------BlockPointer section end ----------\n");
}

void SetGlobalSection()
{
	MasterBlock mb;
	g_disk_capacity = sizeof(MasterBlock); // this will make next line success
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;
	g_disk_capacity = mb.capacity;
}

void ReadWholeDiskToRSCTable()
{
	printf("\n---------------------disk section----------\n");
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;

	FileHeader fh;

	off_t i;
	i = mb.file_header_section;
	while (i < mb.block_pointer_section)
	{
		bzero(&fh, sizeof(FileHeader));
		Read_cache_off(&fh, sizeof(FileHeader), i);

		if (fh.nodeHeader != INVLOC)
		{
			PX_ASSERT(fh.path[0] != '\0');
			PX_ASSERT(fh.path[0] != ' ');

			off_t start = fh.nodeHeader;
			BlockPointerNode bpn;
			while (start != INVLOC)
			{
				Read_cache_off(&bpn, sizeof(BlockPointerNode), start);
				Read_cache_to_RSC_table(&fh, &bpn);
				start = bpn.next;
			}
//			printbn_d(&fh);
		}
		i += sizeof(FileHeader);
	}
	printf("---------------------disk section end ----------\n");
}

void printbnfreelist()
{
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;

	off_t i = mb.block_pointer_flist;
	BlockPointerNode bn;
	while (i != INVLOC)
	{
		Read_cache_off(&bn, sizeof(BlockPointerNode), i);
		i = bn.next;
		printbn(&bn);
	}
}

size_t printspaceleft()
{
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;

	off_t i = mb.block_pointer_flist;
	BlockPointerNode bpn;
	size_t total = 0;
	while (i != INVLOC)
	{
		Read_cache_off(&bpn, sizeof(BlockPointerNode), i);
		i = bpn.next;
		total++;
	}

	printf("num of left blocks %ld, blksize %ld, space %ld\n", total, RSCBLKSIZE, total * RSCBLKSIZE);

	return total * RSCBLKSIZE;
}

void printfileheaderfreelist()
{
#ifdef USEFREAD
	PX_ASSERT(g_cache_fp != NULL);
#else
	PX_ASSERT(g_cache_fp != -1);
#endif
	MasterBlock mb;
	Read_cache_off(&mb, sizeof(MasterBlock), MASTERBLOCKOFF);
	g_block_pointer_section = mb.block_pointer_section;
	g_file_header_section = mb.file_header_section;
	g_block_section = mb.block_section;

	off_t i = mb.file_header_flist;
	FileHeader fh;
	while (i != INVLOC)
	{
		Read_cache_off(&fh, sizeof(FileHeader), i);
		i = fh.next;
		printfh(&fh);
	}
}

void printbn_d(FileHeader* fh)
{
	printf("\n");
	printfh(fh);
	off_t i = fh->nodeHeader;
	BlockPointerNode bn;
	while (i != INVLOC)
	{
		Read_cache_off(&bn, sizeof(BlockPointerNode), i);
		i = bn.next;
		printbn(&bn);
	}
}

void printbn(BlockPointerNode* bn)
{
	char buf[RSCBLKSIZE + 1];
	bzero(buf, RSCBLKSIZE + 1);
	Read_cache_off(buf, RSCBLKSIZE, bn->memloc);
	printf(
			"BlockPointerNode loc %ld, next %ld, fileheader %ld, memloc %ld, fileblockoff %ld, str %.5s\n",
			BLOCKHEADERINDEX(bn->loc), BLOCKHEADERINDEX(bn->next),
			HEADERINDEX(bn->fhloc), BLOCKINDEX(bn->memloc), bn->fileblockoff,
			buf);
}

void printfh(FileHeader* fh)
{
	printf(
			"FileHeader loc %ld, next %ld, nodeHeader %ld, nodeTail %ld, fileoff %ld, path %s\n",
			HEADERINDEX(fh->loc), HEADERINDEX(fh->next),
			BLOCKHEADERINDEX(fh->nodeHeader), BLOCKHEADERINDEX(fh->nodeTail),
			fh->fileoff, (fh->path));
}

void ReadFileOffToCache(const char *path, void* buf, size_t len, off_t off)
{
	FILE* fp = fopen(path, "rb");

	if (fp == NULL)
	{
		printf("path %s not found!\n", path);
		return;
	}

	PX_ASSERT(fp != NULL);
	PX_ASSERT(off + (RSCBLKSIZE - off % RSCBLKSIZE) >= off +len); // off and len cannot cross two cache blocks!

	off_t realoff = off;
	size_t len_w = len;
	size_t left = realoff % RSCBLKSIZE;

	if (left != 0)
	{
		char buf0[RSCBLKSIZE];
		bzero(buf0, RSCBLKSIZE);
		len_w = RSCBLKSIZE;
		realoff = realoff - left;
		assert(realoff % RSCBLKSIZE == 0);
		// TODO read whole cblksize buf from nas
	}

	fseek(fp, realoff, SEEK_SET);
	PX_ASSERT(fread(buf, RSCBLKSIZE, 1, fp)==1);

	Insert_RSC_table(path, buf, len_w, realoff, KEEPCURRENT_F);

	PX_ASSERT(fclose(fp) == 0);
}

void ReadFileFromEndToCache(const char *path)
{
	FILE* fp = fopen(path, "rb");

	if (fp == NULL)
	{
		printf("path %s not found!\n", path);
		return;
	}

	PX_ASSERT(fp != NULL);

	fseek(fp, 0L, SEEK_END);
	off_t filesize = ftell(fp);
	char buf[RSCBLKSIZE + 1];

	off_t i = filesize;
	while (1)
	{
		if (i - i % RSCBLKSIZE > 0)
		{
			fseek(fp, i - RSCBLKSIZE, SEEK_SET);
			bzero(buf, RSCBLKSIZE + 1);
			PX_ASSERT(fread(buf, RSCBLKSIZE, 1, fp)==1);
			Insert_RSC_table(path, buf, RSCBLKSIZE, i - i % RSCBLKSIZE,
					KEEPCURRENT_F);
			i -= RSCBLKSIZE;
		} else
		{
			if (i == 0)
				break;
			fseek(fp, 0, SEEK_SET);
			bzero(buf, RSCBLKSIZE + 1);
			PX_ASSERT(fread(buf, i, 1, fp) == 1);
			Insert_RSC_table(path, buf, i, 0, KEEPCURRENT_F);
			break;
		}
	}
	PX_ASSERT(fclose(fp) == 0);
}

void gettime(char* timestr)
{
	char buf[TIMELEN];
	struct timeval curTime;
	gettimeofday(&curTime, NULL);
	int milli = curTime.tv_usec / 1000;
	strftime(buf, TIMELEN, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));

	sprintf(timestr, "%s:%d", buf, milli);
}

void rsclog(const char *file, int line, const char *func, const char *msg,
		MSGFLAG msg_f)
{
	PX_ASSERT(g_log_fp != NULL);

	char buf[TIMELEN];
	gettime(buf);

	char msg_type[TIMELEN];
	if (msg_f == FATAL_F)
		sprintf(msg_type, "%s", "FATAL");
	else if (msg_f == WARNING_F)
		sprintf(msg_type, "%s", "WARNING");
	else if (msg_f == NORMAL_F)
		sprintf(msg_type, "%s", "NORMAL");
	char str[MAX_MSG];
	sprintf(str, "%s :-%s file %s line %d function %s msg:%s \n", buf, msg_type,
			file, line, func, msg);

	if (msg_f == FATAL_F)
		fprintf(stderr, str);
	(void) fprintf(g_log_fp, "%s", str);
	fflush(g_log_fp);
}

