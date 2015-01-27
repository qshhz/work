#include <RSC.h>
#include <string.h>

PX_BOOL isexp2(off_t val)
{
	if (val < 2)
		return PX_FALSE;

	while(val > 1)
	{
		if (val % 2 == 0)
			val = val/2;
		else
			return PX_FALSE;
	}

	return PX_TRUE;
}


PX_BOOL startswith(char* str, char* substr) {
	int stri = 0;
	int substri = 0;
	int found = INVLOC;
	PX_BOOL spacestart_f = PX_FALSE;
	while (str[stri] != '\0') {
		if (!spacestart_f && str[stri] == ' ') {
			spacestart_f = PX_FALSE;
		} else {
			spacestart_f = PX_TRUE;
			if (substr[substri] == '\0') {
				found = stri;
				break;
			}
			if (substr[substri] != str[stri]) {
				break;
			}
		}

		stri++;
		if (spacestart_f)
			substri++;
	}
//	PX_ASSERT(stri >= strlen(substr));
	return found;
}

void trim(char* str) {
	char tmp[PATH_LEN];
	strcpy(tmp, str);
	bzero(str, PATH_LEN);
	int i = 0;
	int stri = 0;
	PX_BOOL start = PX_FALSE;
	while (tmp[i] != '\0') {
		if (!start && tmp[i] == ' ') {
			start = PX_FALSE;
		} else {
			if (start && tmp[i] == ' ')
				break;
			start = PX_TRUE;
			str[stri++] = tmp[i];
		}
		i++;
	}
}

size_t strtoint(char* tmp) {
	int i = 0;
	int res = 0;
	while (tmp[i] != '\0') {
		if (tmp[i] > '9') {
			fprintf(stderr, "only support decimal!\n");
			PX_ASSERT(0);
		}
		res = tmp[i] - '0' + res * 10;
		i++;
	}
	return res;
}

PX_BOOL readLine(FILE* fp, char* buf) {
	PX_ASSERT(fp != NULL);
	char c;
	int i = 0;
	bzero(buf, PATH_LEN);
	PX_BOOL notend_f = PX_FALSE;
	PX_BOOL valid_f = PX_TRUE;

	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n') {
			notend_f = PX_TRUE;
			valid_f = PX_TRUE;
			break;
		} else if (c == '#') {
			notend_f = PX_TRUE;
			valid_f = PX_FALSE;
//			break;
		} else if (valid_f) {
			buf[i++] = c;
		}
	}

	return notend_f;
}

void readentry(char* entry, char* name, char*tmp) {
	int pathoff = startswith(entry, name);
//	char tmp[PATH_LEN];
	bzero(tmp, PATH_LEN);

	int i = 0;
	while (entry[pathoff] != '\0') {
		tmp[i++] = entry[pathoff++];
	}
	tmp[i] = '\0';

	trim(tmp);
}

void readrscconf(char* path, char* log, size_t *size, off_t *cblksize) {
	FILE* fp = fopen("/px/conf/rsc1.conf", "r");
	PX_ASSERT(fp != NULL);

	char buf[PATH_LEN];
	char tmp[PATH_LEN];
	while (readLine(fp, buf)) {
		readentry(buf, "CMFILE_PATH", tmp);
		if (strlen(tmp) > 0) {
			strcpy(path, tmp);
		}

		readentry(buf, "CMLOG_PATH", tmp);
		if (strlen(tmp) > 0) {
			strcpy(log, tmp);
		}

		if (size) {
			readentry(buf, "CACHESIZE", tmp);
			if (strlen(tmp) > 0) {
				if (tmp[strlen(tmp) - 1] != 'G') {
					fprintf(stderr, "only support Gigabyte!\n");
					PX_ASSERT(0);
				} else {
					tmp[strlen(tmp) - 1] = '\0';
					*size = strtoint(tmp);
				}
			}
		}

		readentry(buf, "CBLKSIZE", tmp);
		if (strlen(tmp) > 0) {
			*cblksize = strtoint(tmp);
		}
	}

	PX_ASSERT(fclose(fp) == 0);
}

