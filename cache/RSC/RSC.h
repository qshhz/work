#ifndef __RSC_H__
#define __RSC_H__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <assert.h>
#include <glib.h>
#include <sys/time.h>

//#define MAX_CMFILE_PATH 512
//#define MAX_CMFILE_PATH 1024
#define CBLKSIZE_REAL 131072l
//1048576
#define TIMELEN 1024
#define PATH_LEN 4096
#define MAX_MSG 1024
// this is for formatting timestamp
#define NUMFILE 10
#define INVLOC -1
#define CACHE_FILE_REAL "/media/SamsungSSD/RSC.cache"
#define CACHE_FILE g_rscfile
#define RSCBLKSIZE g_cblksize
#define MINNUMFILES 100000l
#define MASTERBLOCKOFF 0
#define PX_BOOL int
#define PX_TRUE 1
#define PX_FALSE 0
#define RSCLOG(msg, flag) rsclog(__FILE__, __LINE__, __FUNCTION__, msg, flag)
#define PX_ASSERT(stmt) {PX_BOOL res = (stmt);if(!(res)){RSCLOG("PX_ASSERT", FATAL_F); assert(res);}}


//#define MOUNTPOINT "/px/mfs/10.0.10.10/media/data/rsc_test_dir/dom_NFS_MFS_1"
#define MOUNTPOINT "/px/mfs/10.0.10.91/public_CIFS_2"

#define PERROR(fmt, ...) {\
	char str[MAX_MSG];\
	sprintf(str, fmt, ##__VA_ARGS__);\
	fprintf(stderr, str);}

typedef enum {OVERWRITTEN_F, KEEPCURRENT_F, READDISK_F} INSERTFLAG;
typedef enum {NORMAL_F, WARNING_F, FATAL_F} MSGFLAG;

void rsclog(const char *file, int line, const char *func, const char *msg, MSGFLAG msg_f);
PX_BOOL isexp2(off_t val);



#define BLOCKHEADERINDEX(i) ((i)!=-1? ((i)- g_block_pointer_section) / sizeof(BlockPointerNode):-1)
#define HEADERINDEX(i) ((i)!=-1? ((i)- g_file_header_section) / sizeof(FileHeader):-1)
#define BLOCKINDEX(i) ((i)!=-1? ((i)- g_block_section) / RSCBLKSIZE:-1)

//FILE* g_cache_fp;
int g_cache_fp;
FILE* g_log_fp;
char g_rscfile[PATH_LEN];
char g_logfile[PATH_LEN];
off_t g_cblksize;
pthread_mutex_t g_RSC_freelist_mt;
pthread_mutex_t g_readahead_mt;
pthread_mutexattr_t g_recursive_mt_attr;

off_t g_file_header_section; // use for assert
off_t g_block_pointer_section; // use for assert
off_t g_block_section; // use for assert
off_t g_disk_capacity; // use for assert

/******** in disk *****************/
typedef struct MasterBlock
{
	off_t file_header_section; // use for assert
	off_t file_header_flist;
	off_t file_header_flist_tail;
	off_t block_pointer_section; // use for assert
	off_t block_pointer_flist;
	off_t block_pointer_flist_tail;
	off_t block_section; // use for assert
	off_t capacity; // disk capacity
	off_t cblksize; // cache block size
} MasterBlock;

typedef struct FileHeader
{
#define MAX_MFS_PATH 4096
//#define MAX_MFS_PATH 1047
//	MAX_MFS_PATH 1024 + 23 |"/px/mfs/xxx.xxx.xxx.xxx"
	char path[MAX_MFS_PATH];

	off_t next;
	off_t loc; // location of FileHeader in disk file
	off_t nodeHeader; // location of header BlockPointerNode in disk file
	off_t nodeTail; // location of tail BlockPointerNode in disk file
	off_t fileoff; // this is the offset of in real file  // TODO could be removed later
} FileHeader;

typedef struct BlockPointerNode
{
	off_t next;
	off_t loc; // location of FileNode in disk file
	off_t memloc; // location of BlockPointerNode in disk file
	off_t fhloc; // file header location debugging purpose // TODO could be removed later
	off_t fileblockoff; // this is the offset of block in real file  // TODO could be removed later
} BlockPointerNode;

/*------------------ lock, unlock macro -------------------------*/
#include <sys/syscall.h>
//FILE *mtlogfp;
//FILE *testfp;
/*#define PX_LOCK(mt_addr) fprintf(mtlogfp, "mutex id is %d, owner is %d, try lock thid is %d, at line %d, function %s \n", (mt_addr)->__data.__lock, (mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__); \
fflush(mtlogfp); */
#define PX_LOCK(mt_addr) assert(0 == pthread_mutex_lock(mt_addr));
//fprintf(mtlogfp, "mutex id is %d, owner is %d, locked thid is %d, at line %d, function %s \n", (mt_addr)->__data.__lock, (mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__);
//fflush(mtlogfp);

/*#define PX_UNLOCK(mt_addr) fprintf(mtlogfp, "mutex id is %d, owner is %d , try unlock thid is %d at line %d, function %s \n",(mt_addr)->__data.__lock,(mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__); \
fflush(mtlogfp);\*/
#define PX_UNLOCK(mt_addr) assert(0 == pthread_mutex_unlock(mt_addr));
// fprintf(mtlogfp, "mutex id is %d, owner is %d , unlocked thid is %d at line %d, function %s \n",(mt_addr)->__data.__lock,(mt_addr)->__data.__owner, (int)syscall(SYS_gettid), __LINE__,__FUNCTION__);
//fflush(mtlogfp);

#define PX_INIT_MUTEX(mt_addr) assert(0 == pthread_mutex_init(mt_addr, NULL))
#define PX_INIT_COND(cv_addr) assert(0 == pthread_cond_init(cv_addr, NULL))
#define PX_WAIT(cv_addr, mt_addr) assert(0 == pthread_cond_wait((cv_addr), (mt_addr)))
#define PX_RESUME(cv_addr) assert(0 == pthread_cond_signal(cv_addr))

void Read_cache_off(void* buf, off_t size, off_t off);

void Write_cache_off(void* buf, off_t size, off_t off);

void Return_file_block(BlockPointerNode* bn_p);

void Alloc_file_block(BlockPointerNode* bn_p);

void Return_file_header(FileHeader* fh_p);

void Alloc_file_header(FileHeader* fh_p, const char* path);

void SetGlobalSection();

void printbn(BlockPointerNode* bn);

void printfh(FileHeader* fh);

void printbn_d(FileHeader* fh);

void printWholeDisk();

size_t printspaceleft();

void printWholeBlockPointer();

void printfileheaderfreelist();

void printbnfreelist();

void ReadFileToCache(const char *path, off_t size);
//void ReadFileToCache(const char *path);

void ReadFileFromEndToCache(const char *path);

void ReadFileOffToCache(const char *path, void* buf, size_t len, off_t off);

void Read_cache_to_RSC_table(FileHeader* fh_p, BlockPointerNode* bpn_p);

void ReadWholeDiskToRSCTable();

/******** in ram *****************/

// this is for truncate function to certain size
typedef struct BlockPointerNode_m
{
	BlockPointerNode* bpn;
	struct BlockPointerNode_m* next;
	struct BlockPointerNode_m* pre;
} BlockPointerNode_m;

typedef struct file_block_table_m
{
	GHashTable* tab; // key index of cache block, value
	char * path; // for rename function, this frees stolen key but keep value in hash table
	pthread_mutex_t mt;
	FileHeader* fh_m; // this FileHeader is in ram
//	BlockPointerNode* tail_m; // this BlockPointerNode is in ram
	BlockPointerNode_m* header; // this BlockPointerNode_m header is in ram
	BlockPointerNode_m* tail; // this BlockPointerNode_m tail is in ram
} file_block_table_m;

typedef struct RSC_table_m
{
	GHashTable* tab; // key file mfs path, value file_block_table_m
	pthread_mutex_t mt;
} RSC_table_m;

RSC_table_m* g_RSC_table_m;

off_t* off_tdup(off_t v);

void Cleanup(void* addr);

file_block_table_m* Init_file_block_table();

// this function is for rename since, we need to keep the value but modify the key
void Destory_file_block_table(void* addr);

// only 1 thread call this to init
void Init_RSC_table_m();

// only 1 thread call this to finit
// this function is for rename since, we need to keep the value but modify the key
void Destory_RSC_table_m();

// this will release the old file cache if file name present
void Insert_RSC_table(const char *path, char* buf, size_t len, off_t off, INSERTFLAG insertflag);

// return 1 if found else 0
int Read_RSC_table(const char *path, char* buf, size_t len, off_t off);

// this function is for rename
void Rename_file_block_table(const char *oldfn, const char *newfn);

// this function is for rm
void Rm_file_block_table(const char *path);

// this function is for truncate
void Truncate_file_block_table(const char *path, off_t off);

void printfh_m();

void* Voiddup(const void*from, size_t size);


/*--------------utils------------*/

PX_BOOL startswith(char* str, char* substr);
void trim(char* str);

size_t strtoint(char* tmp);

PX_BOOL readLine(FILE* fp, char* buf);

void readentry(char* entry, char* name, char*tmp);
void readrscconf(char* path, char* log, size_t *size, off_t *cblksize);


/*------------- prefetch-------------*/
typedef enum
{
	PATHDIR_T, PATHFILE_T
} PathType;

typedef struct PathEntry
{
	PathType pt;
	char path[PATH_LEN];
	struct PathEntry* next;
} PathEntry;

typedef struct FetchQueue
{
	const char* mfspath;
	char* flag;
	off_t filesize;
	struct FetchQueue* next;
} FetchQueue;

typedef struct Fetch_files_table
{
	GHashTable* tab; // key index of cache block, value
	pthread_mutex_t mt;
} Fetch_files_table;


typedef struct FetchWorker
{
	pthread_cond_t cv;
	pthread_mutex_t mt;
	pthread_t tid;
	int id;
	int using;
	FetchQueue *que;
	Fetch_files_table* g_fft;

}FetchWorker;
FetchWorker* g_fetchworker;
int g_fetchworker_exit;
#define NUMFETCHWORKER 4l
#define FETCHWORKERSLEEPTIME 20

void fetchDir(const char *name);
void Init_fetch_thread();
void Finit_fetch_thread();
void Block_fetch_thread();
void Wakeup_fetch_thread();

#endif

