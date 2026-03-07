#include <sys/prctl.h>          // [SECURITY] NO_NEW_PRIVS y caps
#include <linux/capability.h>   // [SECURITY] drops CAP_SYS_ADMIN, etc.
#include <signal.h>             // [SECURITY] kill()
#include <sched.h>              // [SECURITY] unshare()

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <asm/unistd.h>

#include <cerrno>
#include <cstdio>               // perror()
#include <cstdlib>              // exit(), EXIT_FAILURE

// for arm64 termux
#ifndef __NR_mknod
#if defined(__aarch64__)
#define __NR_mknod 133
#endif
#endif

void applySeccompFilter() {

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("PR_SET_NO_NEW_PRIVS");
        exit(EXIT_FAILURE);
    }

    struct sock_filter filter[] = {

        // Load syscall number
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),

        // Namespace & FS escape
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_unshare, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mount, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_umount2, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_pivot_root, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // userfaultfd

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_userfaultfd, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Privilege escalation
        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setuid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setgid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setresuid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setresgid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_capset, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Kernel / hardware
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_reboot, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_kexec_load, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_init_module, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_delete_module, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Process inspection
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ptrace, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Keyring
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_keyctl, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_add_key, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_request_key, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Information leaks
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_perf_event_open, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_bpf, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Devices
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mknod, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open_by_handle_at, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // memfd_create

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_memfd_create, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // clone3 and clone

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone3, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // fsopen / fsmount

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fsopen, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fsmount, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // move_mount

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_move_mount, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // open_tree

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open_tree, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // process_vm_readv, process_vm_writev

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_process_vm_readv, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_process_vm_writev, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // setns

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setns, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // name_to_handle_at

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_name_to_handle_at, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // fanotify_init
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fanotify_init, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // fanotify_mark
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fanotify_mark, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Default allow
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        perror("PR_SET_SECCOMP");
        exit(EXIT_FAILURE);
    }
}

// [SECURITY] DROP DANGEROUS CAPABILITIES
void dropDangerousCaps() {
    prctl(PR_CAPBSET_DROP, CAP_SYS_ADMIN, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_MODULE, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_PTRACE, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_BOOT, 0, 0, 0);
    //ussels mabeprctl(PR_CAPBSET_DROP, CAP_SYS_RAWIO, 0, 0, 0);
    prctl(PR_SET_DUMPABLE, 0); // coredumps, gdb
}
