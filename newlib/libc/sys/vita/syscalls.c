#include <errno.h>
#include <fcntl.h>
#include <reent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/unistd.h>

#define SCE_ERRNO_MASK 0xFF
#define MAX_OPEN_FILES 1024

int fd_to_scefd[MAX_OPEN_FILES];
char _newlib_fd_mutex[32] __attribute__ ((aligned (8)));

_ssize_t
_write_r(struct _reent * reent, int fd, const void *buf, size_t nbytes)
{
	int ret;
	if ((unsigned)fd > MAX_OPEN_FILES) {
		reent->_errno = EINVAL;
		return -1;
	}
	if ((fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) 
			&& fd_to_scefd[fd] == 0) {
		ret = nbytes;
	} else {
		ret = sceIoWrite(fd_to_scefd[fd], buf, nbytes);
	}
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	reent->_errno = 0;
	return ret;
}

void
_exit(int rc)
{
	_free_vita_newlib();
	sceKernelExitProcess(rc);
}

int
_close_r(struct _reent *reent, int fd)
{
	if ((unsigned)fd > MAX_OPEN_FILES) {
		reent->_errno = EINVAL;
		return -1;
	}
	reent->_errno = sceIoClose(fd_to_scefd[fd]) & SCE_ERRNO_MASK;
	if (reent->_errno == 0) {
		sceKernelLockLwMutex(_newlib_fd_mutex, 1, 0);
		fd_to_scefd[fd] = 0;
		sceKernelUnlockLwMutex(_newlib_fd_mutex, 1);
	}
	return 0;
}

char *__env[1] = { 0 };
char **environ = __env;

int
_execve_r(struct _reent *reent, const char *name, char * const *argv,
		char * const *env)
{
	reent->_errno = ENOSYS;
	return -1;
}

int
_fork_r(struct _reent *reent)
{
	reent->_errno = ENOSYS;
	return -1;
}

int
_getpid_r(struct _reent *reent)
{
	reent->_errno = 0;
	return sceKernelGetProcessId();
}

int
_gettimeofday_r(struct _reent *reent, struct timeval *ptimeval, void *ptimezone)
{
	int ret = sceKernelLibcGettimeofday(ptimeval, ptimezone);
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	reent->_errno = 0;
	return 0;
}

int
_isatty_r(struct _reent *reent, int file)
{
	reent->_errno = ENOSYS;
	return 0;
}

int
_kill_r(struct _reent *reent, int pid, int sig)
{
	if (pid != sceKernelGetProcessId()) {
		reent->_errno = EPERM;
		return -1;
	}
	switch (sig) {
	default:
		__builtin_trap();
	case SIGINT:
	case SIGTERM:
		sceKernelExitProcess(-sig);
		break;
	case SIGCHLD:
	case SIGCONT:
		return 0;
	}
}

int
_link_r(struct _reent *reent, const char *existing, const char *new)
{
	reent->_errno = ENOSYS;
	return -1;
}

_off_t
_lseek_r(struct _reent *reent, int fd, _off_t ptr, int dir)
{
	int ret;
	if ((unsigned)fd > MAX_OPEN_FILES || fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
		reent->_errno = EINVAL;
		return -1;
	}
	ret = sceIoLseek32(fd_to_scefd[fd], ptr, dir);
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	return ret;
}

int _fcntl2sony(int flags) {
	int out = 0;
	if (flags & O_RDWR)
		out |= 3;
	else if (flags & O_WRONLY)
		out |= 2;
	else
		out |= 1;
	if (flags & O_NONBLOCK)
		out |= 4;
	if (flags & O_APPEND)
		out |= 0x100;
	if (flags & O_CREAT)
		out |= 0x200;
	if (flags & O_TRUNC)
		out |= 0x400;
	if (flags & O_EXCL)
		out |= 0x800;
	return out;
}

int
_open_r(struct _reent *reent, const char *file, int flags, int mode)
{
	int ret, i, found = 0;
	flags = _fcntl2sony(flags);
	ret = sceIoOpen(file, flags, 0666);
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	sceKernelLockLwMutex(_newlib_fd_mutex, 1, 0);
	// skip stdin, stdout, stderr
	for (i = 3; i < MAX_OPEN_FILES; ++i)
		if (fd_to_scefd[i] == 0) {
			found = i;
			break;
		}
	if (!found) {
		sceKernelUnlockLwMutex(_newlib_fd_mutex, 1);
		sceIoClose(ret);
		reent->_errno = EMFILE;
		return -1;
	}
	fd_to_scefd[found] = ret;
	sceKernelUnlockLwMutex(_newlib_fd_mutex, 1);
	reent->_errno = 0;
	return found;
}

_ssize_t
_read_r(struct _reent *reent, int fd, void *ptr, size_t len)
{
	int ret;
	if ((unsigned)fd > MAX_OPEN_FILES) {
		reent->_errno = EINVAL;
		return 01;
	}
	if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
		if (fd_to_scefd[fd] == 0) {
			reent->_errno = 0;
			return 0;
		}
	}
	ret = sceIoRead(fd_to_scefd[fd], ptr, len);
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	reent->_errno = 0;
	return ret;
}

int
_readlink_r(struct _reent *reent, const char *path, char *buf, size_t bufsize)
{
	reent->_errno = ENOSYS;
	return -1;
}

int
_unlink_r(struct _reent *reent, const char * path)
{
	int ret;
	ret = sceIoRemove(path);
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	reent->_errno = 0;
	return 0;
}

int
_rename_r(struct _reent *reent, const char *old, const char *new)
{
	int ret;
	ret = sceIoRename(old, new);
	if (ret < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	reent->_errno = 0;
	return 0;
}

clock_t
_times_r(struct _reent *reent, struct tms *ptms)
{
	unsigned result = sceKernelGetProcessTimeLow();
	ptms->tms_utime = result;
	ptms->tms_stime = 0;
	ptms->tms_cutime = 0;
	ptms->tms_cstime = 0;
	return result;
}

struct SceDateTime {
	unsigned short year;
	unsigned short month;
	unsigned short day;
	unsigned short hour;
	unsigned short minute;
	unsigned short second;
	unsigned int microsecond;
};

struct SceIoStat {
	int st_mode;
	unsigned int st_attr;
	long long st_size;
	struct SceDateTime st_ctime;
	struct SceDateTime st_atime;
	struct SceDateTime st_mtime;
	unsigned st_private[6];
};

enum {
	SCE_DIR = 0x1000,
	SCE_REG = 0x2000,
	SCE_STATFMT = 0xf000
};

#define SCE_ISREG(x) (((x) & SCE_STATFMT) == SCE_REG)
#define SCE_ISDIR(x) (((x) & SCE_STATFMT) == SCE_DIR)

static void
scestat_to_stat(struct SceIoStat *in, struct stat *out) {
	memset(out, 0, sizeof(*out));
	out->st_size = in->st_size;
	if (SCE_ISREG(in->st_mode))
		out->st_mode |= _IFREG;
	if (SCE_ISDIR(in->st_mode))
		out->st_mode |= _IFDIR;
	sceRtcGetTime_t(&in->st_atime, &out->st_atime);
	sceRtcGetTime_t(&in->st_mtime, &out->st_mtime);
	sceRtcGetTime_t(&in->st_ctime, &out->st_ctime);
}

int
_fstat_r(struct _reent *reent, int fd, struct stat *st)
{
	struct SceIoStat stat = {0};
	int ret;
	if ((unsigned)fd > MAX_OPEN_FILES) {
		reent->_errno = EINVAL;
		return -1;
	}
	if ((ret = sceIoGetstatByFd(fd_to_scefd[fd], &stat)) < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	scestat_to_stat(&stat, st);
	reent->_errno = 0;
	return 0;
}

int
_stat_r(struct _reent *reent, const char *path, struct stat *st)
{
	struct SceIoStat stat = {0};
	int ret;
	if ((ret = sceIoGetstat(path, &stat)) < 0) {
		reent->_errno = ret & SCE_ERRNO_MASK;
		return -1;
	}
	scestat_to_stat(&stat, st);
	reent->_errno = 0;
	return 0;
}