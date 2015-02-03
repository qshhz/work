
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <RSC.h>
#include <fcntl.h>
#include <sched.h>


void checkbusy()
{
	PX_LOCK(&g_fetchworker->mt);
	while(g_fetchworker->using)
		pthread_cond_wait(&g_fetchworker->cv, &g_fetchworker->mt);
	PX_UNLOCK(&g_fetchworker->mt);
}

void ReadFileToCache(const char *path, off_t size)
{
	if(size < 1)
	{
		return;
	}

	int fh = open(path, O_RDONLY);

	if (fh == -1)
	{
		printf("path %s not found!\n", path);
		return;
	}

	PX_ASSERT(fh != -1);

	PX_LOCK(&g_RSC_table_m->mt);
	file_block_table_m* fbt = g_hash_table_lookup(g_RSC_table_m->tab, path);
	PX_UNLOCK(&g_RSC_table_m->mt);
	if (fbt != NULL)
	{
		return;
	}


//	size_t filesize = lseek(fh, 0l, SEEK_END);

	size_t filesize = size;
//	char buf[RSCBLKSIZE + 1];
//	lseek(fh, 0l, SEEK_SET);
	void* buf = malloc(RSCBLKSIZE + 1);

	off_t i = 0;
	while (PX_TRUE)
	{
		checkbusy();
		if (filesize == 0)
		{
			break;
		}

		if (i + RSCBLKSIZE < filesize)
		{
			bzero(buf, RSCBLKSIZE + 1);
			size_t nread = pread(fh, buf, RSCBLKSIZE, i);
			PX_ASSERT(nread==RSCBLKSIZE);
			Insert_RSC_table(path, buf, RSCBLKSIZE, i, KEEPCURRENT_F);
			i += RSCBLKSIZE;
		}
		else
		{
			bzero(buf, RSCBLKSIZE + 1);
			size_t nread = pread(fh, buf, filesize - i, i);
			PX_ASSERT(nread==filesize - i);
			Insert_RSC_table(path, buf, filesize - i, i, KEEPCURRENT_F);
			break;
		}
	}
	free(buf);
	PX_ASSERT(close(fh) == 0);
}

void fetchDir(const char *dirname)
{
	DIR *dir;
	struct dirent *entry;

	if (!(dir = opendir(dirname)))
		return;
	if (!(entry = readdir(dir)))
		return;

	do
	{
		checkbusy();
		char path[MAX_MFS_PATH];
		if (entry->d_type == DT_DIR)
		{
			int len = snprintf(path, sizeof(path) - 1, "%s/%s", dirname,
					entry->d_name);
			path[len] = 0;
			if (strcmp(entry->d_name, ".") == 0
					|| strcmp(entry->d_name, "..") == 0)
				continue;
			fetchDir(path);
		}
		else if(entry->d_type == DT_REG)
		{
			sprintf(path, "%s/%s", dirname, entry->d_name);
			ReadFileToCache(path, 0);
		}

		printf("%s\n", path);
//		usleep(200);
	} while ( (entry = readdir(dir)) != NULL);
	closedir(dir);
}

static PathEntry* parseentry(char* buf, PathType pathtype, char*tmp)
{
	PathEntry* pet = NULL;

	if (pathtype == PATHDIR_T)
		readentry(buf, "DIR", tmp);
	else if (pathtype == PATHFILE_T)
		readentry(buf, "FILE", tmp);

	if (strlen(tmp) > 0)
	{
		pet = (PathEntry*)calloc(1l, sizeof(PathEntry));
		pet->pt = pathtype;
		strcpy(pet->path, tmp);
		pet->next = NULL;
	}

	return pet;
}

PathEntry* readfetchconf() // (PathEntry* pe_p)
{
	FILE* fp = fopen("/px/conf/fetch.conf", "r");
	if(fp == NULL)
	{
		return NULL;
	}

	PathEntry* pe = NULL;
	PathEntry* petail = NULL;

	char buf[PATH_LEN];
	while (readLine(fp, buf))
	{
		char tmp[PATH_LEN];
		PathEntry* pet = NULL;
		if((pet = parseentry(buf, PATHDIR_T, tmp)) == NULL)
			pet = parseentry(buf, PATHFILE_T, tmp);

		if(pe == NULL)
		{
			pe = pet;
			petail = pet;
		}
		else
		{
			if(petail)
			{
				petail->next = pet;
				petail = pet;
			}
		}
		continue;
	}

	PX_ASSERT(fclose(fp) == 0);

	return pe;
}

void Block_fetch_thread()
{
	PX_LOCK(&g_fetchworker->mt);
	if(!g_fetchworker->using)
	{
		g_fetchworker->using = 1;
	}
}

void Wakeup_fetch_thread()
{
	g_fetchworker->using = 0;
	pthread_cond_signal(&g_fetchworker->cv);
	PX_UNLOCK(&g_fetchworker->mt);
}

static void* fetchworker(void* arg)
{
	pthread_attr_t thAttr;
	int policy = 0;
	pthread_attr_init(&thAttr);
	pthread_attr_getschedpolicy(&thAttr, &policy);
	pthread_setschedprio(pthread_self(), sched_get_priority_min(policy));
	PX_ASSERT(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)==0);


	FetchWorker* fw = (FetchWorker*)arg;

//	PathEntry* pe = NULL;
	while(PX_TRUE)
	{
		FetchQueue* mfsitem;

		PX_LOCK(&fw->mt);
		while(fw->head == NULL)
		{
			pthread_cond_wait(&fw->cv, &fw->mt);
		}

		mfsitem = fw->head;
		if (mfsitem != NULL)
		{
			fw->head = fw->head->next;
			PX_UNLOCK(&fw->mt);

			/*---------process write back queue---------*/
			ReadFileToCache(mfsitem->mfspath, mfsitem->filesize);
//			char msg[MAX_MSG];
//			sprintf(msg, "\tmfspath: %s -----id is %d------\n", mfsitem->mfspath, fw->id);
//			RSCLOG(msg, NORMAL_F);
//			sleep(10);
			*(mfsitem->flag) = '1' ;

			free(mfsitem);
		}
		else
		{
			fw->tail = NULL;
			PX_UNLOCK(&fw->mt);
		}

		continue;

//		if(fw->id== 0)
//			ReadFileToCache("/px/mfs/10.0.10.10/media/data/rand2G");
//		else
//			ReadFileToCache("/px/mfs/10.0.10.10/media/data/core");
//		sleep(FETCHWORKERSLEEPTIME);
#ifdef USEFETCHCONF
		pe = readfetchconf();
		while (pe != NULL)
		{
			if (pe->pt == PATHDIR_T)
			{
				fetchDir(pe->path);
			}
			else if (pe->pt == PATHFILE_T)
			{
				ReadFileToCache(pe->path);
			}
			PathEntry* freepe = pe;
			pe = pe->next;
			free(freepe);
		}

		printf("done fetchworker!\n");
		sleep(FETCHWORKERSLEEPTIME);
#endif
	}
	pthread_exit(NULL);
}

void Finit_fetch_thread()
{
	FetchWorker* fw = g_fetchworker;
//	fw->que = NULL;

	int i=0;
	while(i < NUMFETCHWORKER)
	{
		PX_ASSERT(pthread_cancel(fw->tid)==0);
		fw++;
		i++;
	}

	g_hash_table_destroy(g_fetchworker->g_fft->tab);

	free(g_fetchworker);
}

void Init_fetch_thread()
{
	g_fetchworker = (FetchWorker*) calloc(NUMFETCHWORKER, sizeof(FetchWorker));

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	Fetch_files_table* ffs = (Fetch_files_table*) calloc(1l, sizeof(Fetch_files_table));
	ffs->tab = g_hash_table_new_full(g_str_hash, g_str_equal, free,
			free); // Destory_file_block_table will free key space
	PX_ASSERT(pthread_mutex_init(&ffs->mt, NULL) == 0);

	FetchWorker* fw = g_fetchworker;
	int i=0;
	while(i < NUMFETCHWORKER)
	{
		PX_ASSERT(pthread_create(&fw->tid, &attr, fetchworker, fw) == 0);
		PX_ASSERT(pthread_mutex_init(&fw->mt, NULL) == 0);
		PX_ASSERT(pthread_cond_init(&fw->cv, NULL) == 0);
		fw->using = 0;
		fw->id = i;
		fw->g_fft = ffs;

		fw++;
		i++;
	}


}

void Init_fetch_thread1()
{
	g_fetchworker = (FetchWorker*) calloc(1l, sizeof(FetchWorker));
	PX_ASSERT(pthread_create(&g_fetchworker->tid, NULL, fetchworker, g_fetchworker) == 0);
	PX_ASSERT(pthread_mutex_init(&g_fetchworker->mt, NULL) == 0);
	PX_ASSERT(pthread_cond_init(&g_fetchworker->cv, NULL) == 0);
	g_fetchworker->using = 0;
}
