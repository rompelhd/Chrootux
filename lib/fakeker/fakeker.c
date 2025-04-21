#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/auxv.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _UTSNAME_SYSNAME_LENGTH
#define _UTSNAME_SYSNAME_LENGTH 65
#endif

#ifndef _UTSNAME_VERSION_LENGTH
#define _UTSNAME_VERSION_LENGTH 65
#endif

#ifndef _UTSNAME_RELEASE_LENGTH
#define _UTSNAME_RELEASE_LENGTH 65
#endif

int fkernel(struct utsname *un) {
    int ret = syscall(SYS_uname, un);
    if (ret != 0) {
        perror("syscall SYS_uname failed");
        return ret;
    }

    const char* fake_release = getenv("FAKE_KERNEL_RELEASE");
    const char* fake_sysname = getenv("FAKE_SYSNAME");
    const char* fake_version = getenv("FAKE_VERSION");
    const char* debug = getenv("DEBUG_FAKE_UNAME");

    if (debug && strcmp(debug, "1") == 0) {
        fprintf(stderr, "FAKE_KERNEL_RELEASE=%s\n", fake_release);
        fprintf(stderr, "Original release: %s\n", un->release);
    }

    if (fake_sysname) snprintf(un->sysname, _UTSNAME_SYSNAME_LENGTH, "%s", fake_sysname);
    if (fake_version) snprintf(un->version, _UTSNAME_VERSION_LENGTH, "%s", fake_version);
    if (fake_release) snprintf(un->release, _UTSNAME_RELEASE_LENGTH, "%s", fake_release);

    return ret;
}

extern int uname(struct utsname *un) __attribute__((alias("fkernel")));

unsigned long fgetauxval(unsigned long type) {
    typedef unsigned long (*orig_getauxval_t)(unsigned long);
    orig_getauxval_t orig_getauxval = (orig_getauxval_t)dlsym(RTLD_NEXT, "getauxval");

    const char* debug = getenv("DEBUG_FAKE_GETAUXVAL");
    const char* fake_hwcap = getenv("FAKE_HWCAP");
    const char* fake_hwcap2 = getenv("FAKE_HWCAP2");

    if (debug && strcmp(debug, "1") == 0) {
        fprintf(stderr, "Intercepted getauxval type: %lu\n", type);
    }

    if (type == AT_HWCAP && fake_hwcap) {
        return strtoul(fake_hwcap, NULL, 0);
    }
    if (type == AT_HWCAP2 && fake_hwcap2) {
        return strtoul(fake_hwcap2, NULL, 0);
    }

    return orig_getauxval(type);
}

extern unsigned long getauxval(unsigned long type) __attribute__((alias("fgetauxval")));

static int intercepted_version = 0;

int open(const char *pathname, int flags, ...) {
    typedef int (*orig_open_t)(const char *, int, ...);
    orig_open_t orig_open = (orig_open_t)dlsym(RTLD_NEXT, "open");

    const char* debug = getenv("DEBUG_FAKE_PROC");

    if (strcmp(pathname, "/proc/version") == 0 && !intercepted_version) {
        if (debug && strcmp(debug, "1") == 0) {
            fprintf(stderr, "Intercepted open for /proc/version\n");
        }
        intercepted_version = 1;
        return orig_open("/fake_proc_version", flags);
    }

    return orig_open(pathname, flags);
}

ssize_t read(int fd, void *buf, size_t count) {
    typedef ssize_t (*orig_read_t)(int, void *, size_t);
    orig_read_t orig_read = (orig_read_t)dlsym(RTLD_NEXT, "read");

    ssize_t bytes = orig_read(fd, buf, count);

    const char* fake_version = getenv("FAKE_KERNEL_VERSION");
    const char* debug = getenv("DEBUG_FAKE_PROC");

    if (fake_version && strstr((char *)buf, "Linux version") && !intercepted_version) {
        if (debug && strcmp(debug, "1") == 0) {
            fprintf(stderr, "Modifying read output for /proc/version\n");
        }
        intercepted_version = 1;
        snprintf((char *)buf, count, "Linux version %s\n", fake_version);
        bytes = strlen((char *)buf);
    }

    return bytes;
}
