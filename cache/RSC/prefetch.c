
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <RSC.h>
#include <fcntl.h>



void checkbusy()
{
	PX_LOCK(&g_fetchworker->mt);
	while(g_fetchworker->using)
		pthread_cond_wait(&g_fetchworker->cv, &g_fetchworker->mt);
	PX_UNLOCK(&g_fetchworker->mt);
}


void ReadFileToCache(const char *path)
{
	int fh = open(path, O_RDONLY);

	if (fh == -1)
	{
		printf("path %s not found!\n", path);
		return;
	}

	PX_ASSERT(fh != -1);

	size_t filesize = lseek(fh, 0l, SEEK_END);
//	char buf[RSCBLKSIZE + 1];
	lseek(fh, 0l, SEEK_SET);
	void* buf = malloc(RSCBLKSIZE + 1);

//	lseek(fh, 0L, SEEK_SET);
	off_t i = 0;
	while (1)
	{
		checkbusy();
		if(filesize == 0)
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
		} else
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
			ReadFileToCache(path);
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
			petail->next = pet;
			petail = pet;
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
	PathEntry* pe = NULL;
	while(PX_TRUE)
	{
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
	}
	pthread_exit(NULL);
}

void Init_fetch_thread()
{
	g_fetchworker = (FetchWorker*) calloc(1l, sizeof(FetchWorker));
	PX_ASSERT(pthread_create(&g_fetchworker->tid, NULL, fetchworker, g_fetchworker) == 0);
	PX_ASSERT(pthread_mutex_init(&g_fetchworker->mt, NULL) == 0);
	PX_ASSERT(pthread_cond_init(&g_fetchworker->cv, NULL) == 0);
	g_fetchworker->using = 0;
}
