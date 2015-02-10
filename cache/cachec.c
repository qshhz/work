/*
 FUSE: Filesystem in Userspace
 Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

 This program can be distributed under the terms of the GNU GPL.
 See the file COPYING.

 gcc -Wall fusecm_fh.c `pkg-config fuse --cflags --libs` -lulockmgr -o fusecm_fh
 */

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#define _GNU_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define HAVE_SETXATTR

#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <sys/file.h> /* flock(2) */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <RSC.h>
pthread_mutex_t g_read_mt;

//#define MOUNTPOINT "/home/alex/Desktop/mnt/fuse"
//#define MOUNTPOINT "/px/mfs/10.0.10.10/media/data"
//#define MOUNTPOINT "/media/parsecdata/oridir/gai"

void getmfspath(const char *path, char* mfspath)
{
	sprintf(mfspath, "%s%s", MOUNTPOINT, path);
}


#define GETMFSPATH() {char mfspath[MAX_MFS_PATH];\
	sprintf(mfspath, "%s%s%c", MOUNTPOINT, path, '\0');\
	path = mfspath;}

static int cm_getattr(const char *path, struct stat *stbuf)
{
	int res = -1;

	GETMFSPATH();

	res = lstat(path, stbuf);

	if (res == -1)
		return -errno;
#if 0
	if (S_ISREG(stbuf->st_mode))
	{
		FetchQueue* fq = NULL;

		PX_LOCK(&g_fetchworker->g_fft->mt);
		if (g_hash_table_lookup(g_fetchworker->g_fft->tab, path) == NULL)
		{
			char* mfspath = strdup(path);
			fq = calloc(1l, sizeof(FetchQueue));
			fq->mfspath = mfspath;
			fq->flag = 	calloc(1l, sizeof(char));
			fq->filesize = stbuf->st_size;
			g_hash_table_insert(g_fetchworker->g_fft->tab, mfspath, fq->flag);
		}
		PX_UNLOCK(&g_fetchworker->g_fft->mt);

		if (fq != NULL)
		{
			int i = g_hash_table_size(g_fetchworker->g_fft->tab) % NUMFETCHWORKER;
			FetchWorker* fw = g_fetchworker + i;

			PX_LOCK(&fw->mt);
			if(fw->head == NULL)
			{
				fw->head = fq;
				fw->tail = fq;
			}
			else
			{
				fw->tail->next = fq;
				fw->tail = fw->tail->next;
			}
			pthread_cond_signal(&fw->cv);
			PX_UNLOCK(&fw->mt);
		}
	}
#endif

	return 0;
}

static int cm_fgetattr(const char *path, struct stat *stbuf,
		struct fuse_file_info *fi)
{
	int res;

	(void) path;

	struct fuse_file_info* fim = fi;

	res = fstat(fim->fh, stbuf);

	if (res == -1)
		return -errno;

	return 0;
}

static int cm_access(const char *path, int mask)
{
	int res;

	GETMFSPATH();

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_readlink(const char *path, char *buf, size_t size)
{
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}

struct cm_dirp
{
	DIR *dp;
	struct dirent *entry;
	off_t offset;
};

static int cm_opendir(const char *path, struct fuse_file_info *fi)
{
	int res;
	struct cm_dirp *d = malloc(sizeof(struct cm_dirp));
	if (d == NULL)
		return -ENOMEM;

	GETMFSPATH();

	d->dp = opendir(path);
	if (d->dp == NULL)
	{
		res = -errno;
		free(d);
		return res;
	}
	d->offset = 0;
	d->entry = NULL;

	fi->fh = (unsigned long) d;
	return 0;
}

static inline struct cm_dirp *get_dirp(struct fuse_file_info *fi)
{
	return (struct cm_dirp *) (uintptr_t) fi->fh;
}

static int cm_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi)
{
	struct cm_dirp *d = get_dirp(fi);

	(void) path;
	if (offset != d->offset)
	{
		seekdir(d->dp, offset);
		d->entry = NULL;
		d->offset = offset;
	}
	while (1)
	{
		struct stat st;
		off_t nextoff;

		if (!d->entry)
		{
			d->entry = readdir(d->dp);
			if (!d->entry)
				break;
		}

		memset(&st, 0, sizeof(st));
		st.st_ino = d->entry->d_ino;
		st.st_mode = d->entry->d_type << 12;
		nextoff = telldir(d->dp);
		if (filler(buf, d->entry->d_name, &st, nextoff))
			break;

		d->entry = NULL;
		d->offset = nextoff;
	}

	return 0;
}

static int cm_releasedir(const char *path, struct fuse_file_info *fi)
{
	struct cm_dirp *d = get_dirp(fi);
	(void) path;
	closedir(d->dp);
	free(d);
	return 0;
}

static int cm_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	GETMFSPATH();

	if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_mkdir(const char *path, mode_t mode)
{
	int res;

	GETMFSPATH();

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_unlink(const char *path)
{
	int res;

	GETMFSPATH();

	res = unlink(path);
	if (res == -1)
		return -errno;

	/*------------------gai--rsc---------------------*/
	// add check if its read side cached, seemed unnecessary since its path will not be in hash table
	Rm_file_block_table(path);// remove the file RSC if its read cached

	return 0;
}

static int cm_rmdir(const char *path)
{
	int res;

	GETMFSPATH();

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_symlink(const char *from, const char *to)
{
	int res;

	char mfspathfrom[MAX_MFS_PATH];
	char mfspathto[MAX_MFS_PATH];
	getmfspath(from, mfspathfrom);
	getmfspath(to, mfspathto);
	from = mfspathfrom;
	to = mfspathto;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_rename(const char *from, const char *to)
{
	int res;

	char mfspathfrom[MAX_MFS_PATH];
	char mfspathto[MAX_MFS_PATH];
	getmfspath(from, mfspathfrom);
	getmfspath(to, mfspathto);
	from = mfspathfrom;
	to = mfspathto;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_link(const char *from, const char *to)
{
	int res;

//	char* path = from;
	char mfspathfrom[MAX_MFS_PATH];
	char mfspathto[MAX_MFS_PATH];

	getmfspath(from, mfspathfrom);
	getmfspath(to, mfspathto);
	from = mfspathfrom;
	to = mfspathto;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_chmod(const char *path, mode_t mode)
{
	int res;

	GETMFSPATH();

	res = chmod(path, mode);

	if (res == -1)
		return -errno;

//	file_block_table_m* fbt = Lookup_file_RSC_Table(path);
//	if(fbt != NULL)
//	{
//		PX_ASSERT(lstat(path, &fbt->fh_m->stbuf) != -1);
//		//		Release_file()
//		PX_UNLOCK(&fbt->mt);
//	}

	return 0;
}

static int cm_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	GETMFSPATH();

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

//	file_block_table_m* fbt = Lookup_file_RSC_Table(path);
//	if(fbt != NULL)
//	{
//		PX_ASSERT(lstat(path, &fbt->fh_m->stbuf) != -1);
//		//		Release_file()
//		PX_UNLOCK(&fbt->mt);
//	}

	return 0;
}

static int cm_truncate(const char *path, off_t size)
{
	int res;

	GETMFSPATH();

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	Truncate_file_block_table(path, size);

	return 0;
}

static int cm_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	int res;

	(void) path;

	struct fuse_file_info* fim = fi;

	res = ftruncate(fim->fh, size);
	if (res == -1)
		return -errno;

	GETMFSPATH();
	Truncate_file_block_table(path, size);

	return 0;
}

static int cm_utimens(const char *path, const struct timespec ts[2])
{
	int res;

	GETMFSPATH();

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

//	file_block_table_m* fbt = Lookup_file_RSC_Table(path);
//	if(fbt != NULL)
//	{
//		PX_ASSERT(lstat(path, &fbt->fh_m->stbuf) != -1);
//		//		Release_file()
//		PX_UNLOCK(&fbt->mt);
//	}
	return 0;
}

static int cm_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int fd;

	GETMFSPATH();

	fd = open(path, fi->flags, mode);
	if (fd == -1)
		return -errno;

//	struct fuse_file_info* fim = fi;//(struct fuse_file_info*)malloc(sizeof(FileInfo_m));
	fi->fh = fd;
//	fim->path = path;

//	fi->fh = (uint64_t)fim;
//	fi->fh = fd;
	return 0;
}

static int cm_open(const char *path, struct fuse_file_info *fi)
{
	int fd;

	GETMFSPATH();

	fd = open(path, fi->flags);
	if (fd == -1)
		return -errno;

//	struct fuse_file_info* fim = fi;//(FileInfo_m*)malloc(sizeof(FileInfo_m));
	fi->fh = fd;

//	fi->fh = (uint64_t)fim;
	return 0;
}

static int cm_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int res = 0;
	(void) path;
	GETMFSPATH();

	off_t len = size;
	off_t nreadtotal = 0;

	while(len > 0)
	{
		off_t left = offset % RSCBLKSIZE;

		off_t nread = len;
		if(left + len > RSCBLKSIZE)
		{
			nread = (RSCBLKSIZE - left);
		}
		res = 0;

		/*---------- read from rsctable or nas ----------*/
		PX_LOCK(&g_readahead_mt);
		ssize_t found = Read_RSC_table(path, buf, nread, offset);
		if (found != -1)
		{
			res = found;
		}
		else
		{
			PX_LOCK(&g_fetchworker->mt);
			if(!g_fetchworker->using)
			{
				g_fetchworker->using = 1;
			}

			char buf_t[RSCBLKSIZE];
			bzero(buf_t, RSCBLKSIZE);
			ssize_t curoffset = offset % RSCBLKSIZE;
			size_t realoff = offset - curoffset;
			off_t res_t = pread(fi->fh, buf_t, RSCBLKSIZE, realoff);
			if (res_t != -1)
			{
				if(res_t > 0)
				{
					Insert_RSC_table(path, buf_t, res_t, realoff, KEEPCURRENT_F);
					if (res_t - curoffset > 0)
					{
						int i = 0;
						for (; i < nread && i < res_t - curoffset; i++)
						{
							*(buf + i) = buf_t[i + curoffset];
							res++;
						}
					}
				}
			}
			else
			{
				g_fetchworker->using = 0;
				pthread_cond_signal(&g_fetchworker->cv);
				PX_UNLOCK(&g_fetchworker->mt);
				return -errno;
			}

			g_fetchworker->using = 0;
			pthread_cond_signal(&g_fetchworker->cv);
			PX_UNLOCK(&g_fetchworker->mt);
		}
		PX_UNLOCK(&g_readahead_mt);

		/*-------- caculate read offset ----------*/
		nreadtotal += res;
		buf  += res;

		if(left + len > RSCBLKSIZE)
		{
			len -= nread;
			offset += nread;
		}
		else
		{
			len = 0;
		}
	}

	return nreadtotal;
}

//static int cm_read(const char *path, char *buf, size_t size, off_t offset,
//		struct fuse_file_info *fi)
//{
//	int res = 0;
//	off_t nreadtotal = 0;
//
//	(void) path;
//
//	GETMFSPATH();
//
//	off_t toread = size;
//	while (toread > 0)
//	{
//		size_t thisoff = offset % RSCBLKSIZE;
//		size_t thislen = RSCBLKSIZE - thisoff;
//		if (thislen > toread)
//		{
//			thislen = toread;
//		}
//
//		if (Read_RSC_table(path, buf, thislen, offset))
//		{
//			res = thislen;
//			nreadtotal += thislen;
//		}
//		else
//		{
//			char buf_t[RSCBLKSIZE];
//			size_t realoff = offset - offset % RSCBLKSIZE;
//			res = pread(fi->fh, buf_t, RSCBLKSIZE, offset - offset % RSCBLKSIZE);
//			if (res == -1)
//				res = -errno;
//			PX_ASSERT(res != -1);
//			nreadtotal += res;
//			if(res == 0) // reach end of the file
//			{
//				res = thislen;
//				break;
//			}
//			memcpy(buf, buf_t+(offset % RSCBLKSIZE), thislen);
////			res = pread(fi->fh, buf, thislen, offset);
//			PX_ASSERT(offset - offset % RSCBLKSIZE + RSCBLKSIZE >= offset +size); // off and len cannot cross two cache blocks!
//			Insert_RSC_table(path, buf_t, RSCBLKSIZE, realoff, KEEPCURRENT_F);
//		}
//
//		buf += thislen;
//		toread -= thislen;
//		offset += thislen;
//	}
//
////	if (Read_RSC_table(path, buf, size, offset))
////	{
//////		char buf_t[RSCBLKSIZE];
//////		pread(fi->fh, buf_t, size, offset);
//////		int i=0;
//////		if ( strcmp(buf, buf_t) != 0)
//////			for(;buf_t[i]!='\0';i++)
//////			{
//////				printf("cache %c, real %c",buf_t[i], buf[i] );
//////			}
////
//////		strcpy(buf, buf_t);
////		res = size;
////
////	} else
////	{
////		struct fuse_file_info* fi_m = fi;
////		res = pread(fi_m->fh, buf, size, offset);
////		PX_ASSERT(offset - offset % RSCBLKSIZE + RSCBLKSIZE >= offset +size); // off and len cannot cross two cache blocks!
//////		PX_ASSERT(offset + (RSCBLKSIZE - size % RSCBLKSIZE) >= offset +size); // off and len cannot cross two cache blocks!
////		Insert_RSC_table(path, buf, size, offset, KEEPCURRENT_F);
////	}
////	res = pread(fi->fh, buf, size, offset);
////
////	FILE* f = fopen(path, "r");
////	fread(buf, size, 1, f);
////	fclose(f);
//
//
//	return nreadtotal;
//}

static int cm_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec *src;

	src = malloc(sizeof(struct fuse_bufvec));
	if (src == NULL)
		return -ENOMEM;

	*src = FUSE_BUFVEC_INIT(size);

	src->buf->mem = malloc(size);

	if (src->buf->mem == NULL)
		return -errno;

	size_t ret = cm_read(path, src->buf->mem, size, offset, fi);
	if (ret < 0)
	{
		if (!errno)
			errno = EIO;
		return -errno;
	}

	src->buf->size = ret;
	*bufp = src;

	return 0;
}

static int cm_write(const char *path, const char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int res;

	(void) path;

	GETMFSPATH();
//	struct statvfs stbuf_t;
//	PX_ASSERT(-1 != statvfs(MOUNTPOINT, &stbuf_t));
//	PX_ASSERT(-1 != statvfs(path, &stbuf_t));
//	if(stbuf_t.f_bavail <= 538346748)
//	{
//		printf("test");
//	}

	res = pwrite(fi->fh, buf, size, offset);

	Insert_RSC_table(path, (char*)buf, size, offset, OVERWRITTEN_F);

	if (res == -1)
		res = -errno;

	return size;
}

static int cm_write_buf(const char *path, struct fuse_bufvec *buf, off_t offset,
		struct fuse_file_info *fi)
{
	struct fuse_buf *fb = &buf->buf[0];

	return cm_write(path, fb->mem, fb->size, offset, fi);
}

static int cm_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	GETMFSPATH();

	res = statvfs(path, stbuf);
//	struct statvfs stbuf_t;
//	res = statvfs("/media/parsecdata", &stbuf_t);
//	stbuf->f_blocks += stbuf_t.f_blocks*(stbuf_t.f_bsize*1.0/stbuf->f_bsize);
//	stbuf->f_bavail += stbuf_t.f_bavail*(stbuf_t.f_bsize*1.0/stbuf->f_bsize);
//	stbuf->f_bfree += stbuf_t.f_bfree*(stbuf_t.f_bsize*1.0/stbuf->f_bsize);
//
//	printf("blocks %ld, avail %ld, free %ld\n",  stbuf_t.f_blocks,  stbuf_t.f_bavail,  stbuf_t.f_bfree);

	if (res == -1)
		return -errno;

	return 0;
}

static int cm_flush(const char *path, struct fuse_file_info *fi)
{
	int res;

	(void) path;
	/* This is called from every close on an open file, so call the
	 close on the underlying filesystem.	But since flush may be
	 called multiple times for an open file, this must not really
	 close the file.  This is important if used on a network
	 filesystem like NFS which flush the data/metadata on close() */
//	struct fuse_file_info* fim = fi;
	res = close(dup(fi->fh));
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	struct fuse_file_info* fim = fi;
	close(fim->fh);
//	free(fim);

	return 0;
}

static int cm_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	int res;
	(void) path;

#ifndef HAVE_FDATASYNC
	(void) isdatasync;
#else
	if (isdatasync)
	res = fdatasync(fi->fh);
	else
#endif

	struct fuse_file_info* fim = fi;
	res = fsync(fim->fh);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int cm_fallocate(const char *path, int mode,
		off_t offset, off_t length, struct fuse_file_info *fi)
{
	(void) path;

	if (mode)
	return -EOPNOTSUPP;

	return -posix_fallocate(fi->fh, offset, length);
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int cm_setxattr(const char *path, const char *name, const char *value,
		size_t size, int flags)
{
	GETMFSPATH();
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int cm_getxattr(const char *path, const char *name, char *value,
		size_t size)
{
	GETMFSPATH();
//	int res = lgetxattr(path, name, value, size);
	int res = getxattr(path,"user.POSIX_ACL_ACCESS", value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int cm_listxattr(const char *path, char *list, size_t size)
{
	GETMFSPATH();
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int cm_removexattr(const char *path, const char *name)
{
	GETMFSPATH();
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

pthread_mutex_t file_mt;
static int cm_lock(const char *path, struct fuse_file_info *fi, int cmd,
		    struct flock *lock)
{
	(void) path; // fcntl ulockmgr_op
	PX_LOCK(&file_mt);

	int res = fcntl(fi->fh, cmd, lock, &fi->lock_owner,
			   sizeof(fi->lock_owner));

	PX_UNLOCK(&file_mt);

	return res;
}

static int cm_flock(const char *path, struct fuse_file_info *fi, int op)
{
	int res;
	(void) path;

	res = flock(fi->fh, op);
	if (res == -1)
		return -errno;

	return 0;
}

static int cm_ioctl(const char *path, int cmd, void *arg,
		      struct fuse_file_info *fi, unsigned int flags, void *data)
{
	(void) arg;
	(void) fi;
	(void) flags;

//	if (fioc_file_type(path) != FIOC_FILE)
//		return -EINVAL;
//
//	if (flags & FUSE_IOCTL_COMPAT)
//		return -ENOSYS;
//
	switch (cmd) {
	case PX_IOCTL_SEEDRSC:
		printf("PX_IOCTL_SEEDRSC detected! %s\n", path);
//		char mfspath[MAX_MFS_PATH];
//		(*cmctx->fuseops.ioctl)(path, PX_IOCTL_GET_MFSPATH, NULL, NULL, 0, mfspath);
//		QueueFileToRSC(stat, mfspath);

		GETMFSPATH();
		struct stat stbuf;
		lstat(path, &stbuf);
		QueueFileToRSC(&stbuf, path);

		return 0;

	case PX_IOCTL_UPDATE_CACHE:;
		cm_cache_io_t* t = (cm_cache_io_t *)data;
		t->i++;
		t->j++;
//		fioc_resize(*(size_t *)data);
		return 0;
	}

	return 0;
//	return -EINVAL;
}


static void *cm_init(struct fuse_conn_info *conn)
{
//	Init_thd(WBT *wbt)
	PX_ASSERT(pthread_mutex_init(&file_mt, NULL) == 0);
	Init_RSC_table_m();
	PX_ASSERT(pthread_mutex_init(&g_read_mt, &g_recursive_mt_attr) == 0);
	return NULL;
}

static void cm_destroy(void *arg)
{
	Destory_RSC_table_m();
}

static struct fuse_operations cm_oper = {
	.getattr	= cm_getattr,
	.fgetattr	= cm_fgetattr,
	.access		= cm_access,
	.readlink	= cm_readlink,
	.opendir	= cm_opendir,
	.readdir	= cm_readdir,
	.releasedir	= cm_releasedir,
	.mknod		= cm_mknod,
	.mkdir		= cm_mkdir,
	.symlink	= cm_symlink,
	.unlink		= cm_unlink,
	.rmdir		= cm_rmdir,
	.rename		= cm_rename,
	.link		= cm_link,
	.chmod		= cm_chmod,
	.chown		= cm_chown,
	.truncate	= cm_truncate,
	.ftruncate	= cm_ftruncate,
#if 1
	.utimens	= cm_utimens,
#endif
	.create		= cm_create,
	.open		= cm_open,
	.read		= cm_read,
	.read_buf	= cm_read_buf,
	.write		= cm_write,
	.write_buf	= cm_write_buf,
	.statfs		= cm_statfs,
	.flush		= cm_flush,
	.release	= cm_release,
	.fsync		= cm_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= cm_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= cm_setxattr,
	.getxattr	= cm_getxattr,
	.listxattr	= cm_listxattr,
	.removexattr	= cm_removexattr,
#endif
	.lock		= cm_lock,///,  blksize=131072 -d -o nonempty -o  big_writes,max_write=131072
	.flock		= cm_flock,

	.flag_nullpath_ok = 1,
	.ioctl		= cm_ioctl,
	.init		= cm_init,
	.destroy	= cm_destroy,
#if HAVE_UTIMENSAT
	.flag_utime_omit_ok = 1,
#endif
};


int main(int argc, char *argv[])
{
	umask(0);
	system("umount /public_CIFS_2");
//	system("./mkrcf -r");
	return fuse_main(argc, argv, &cm_oper, NULL);
}
