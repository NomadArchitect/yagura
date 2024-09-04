#pragma once

// System calls in Linux 5.1 that are not implemented by this kernel.
// See arch/x86/entry/syscalls/syscall_32.tbl in the Linux kernel source.
#define ENUMERATE_UNIMPLEMENTED_SYSCALLS(F)                                    \
    F(restart_syscall)                                                         \
    F(creat)                                                                   \
    F(time)                                                                    \
    F(chmod)                                                                   \
    F(lchown)                                                                  \
    F(oldstat)                                                                 \
    F(umount)                                                                  \
    F(setuid)                                                                  \
    F(getuid)                                                                  \
    F(stime)                                                                   \
    F(ptrace)                                                                  \
    F(alarm)                                                                   \
    F(oldfstat)                                                                \
    F(pause)                                                                   \
    F(utime)                                                                   \
    F(access)                                                                  \
    F(nice)                                                                    \
    F(sync)                                                                    \
    F(dup)                                                                     \
    F(brk)                                                                     \
    F(setgid)                                                                  \
    F(getgid)                                                                  \
    F(signal)                                                                  \
    F(geteuid)                                                                 \
    F(getegid)                                                                 \
    F(acct)                                                                    \
    F(umount2)                                                                 \
    F(oldolduname)                                                             \
    F(umask)                                                                   \
    F(chroot)                                                                  \
    F(ustat)                                                                   \
    F(getppid)                                                                 \
    F(getpgrp)                                                                 \
    F(setsid)                                                                  \
    F(sgetmask)                                                                \
    F(ssetmask)                                                                \
    F(setreuid)                                                                \
    F(setregid)                                                                \
    F(sethostname)                                                             \
    F(setrlimit)                                                               \
    F(getrlimit)                                                               \
    F(getrusage)                                                               \
    F(gettimeofday)                                                            \
    F(settimeofday)                                                            \
    F(getgroups)                                                               \
    F(setgroups)                                                               \
    F(select)                                                                  \
    F(oldlstat)                                                                \
    F(uselib)                                                                  \
    F(swapon)                                                                  \
    F(readdir)                                                                 \
    F(mmap)                                                                    \
    F(truncate)                                                                \
    F(fchmod)                                                                  \
    F(fchown)                                                                  \
    F(getpriority)                                                             \
    F(setpriority)                                                             \
    F(statfs)                                                                  \
    F(fstatfs)                                                                 \
    F(ioperm)                                                                  \
    F(socketcall)                                                              \
    F(syslog)                                                                  \
    F(setitimer)                                                               \
    F(getitimer)                                                               \
    F(fstat)                                                                   \
    F(olduname)                                                                \
    F(iopl)                                                                    \
    F(vhangup)                                                                 \
    F(vm86old)                                                                 \
    F(wait4)                                                                   \
    F(swapoff)                                                                 \
    F(sysinfo)                                                                 \
    F(ipc)                                                                     \
    F(fsync)                                                                   \
    F(setdomainname)                                                           \
    F(modify_ldt)                                                              \
    F(adjtimex)                                                                \
    F(mprotect)                                                                \
    F(init_module)                                                             \
    F(delete_module)                                                           \
    F(quotactl)                                                                \
    F(fchdir)                                                                  \
    F(bdflush)                                                                 \
    F(sysfs)                                                                   \
    F(personality)                                                             \
    F(setfsuid)                                                                \
    F(setfsgid)                                                                \
    F(_llseek)                                                                 \
    F(_newselect)                                                              \
    F(flock)                                                                   \
    F(msync)                                                                   \
    F(readv)                                                                   \
    F(writev)                                                                  \
    F(getsid)                                                                  \
    F(fdatasync)                                                               \
    F(_sysctl)                                                                 \
    F(mlock)                                                                   \
    F(munlock)                                                                 \
    F(mlockall)                                                                \
    F(munlockall)                                                              \
    F(sched_setparam)                                                          \
    F(sched_getparam)                                                          \
    F(sched_setscheduler)                                                      \
    F(sched_getscheduler)                                                      \
    F(sched_get_priority_max)                                                  \
    F(sched_get_priority_min)                                                  \
    F(sched_rr_get_interval)                                                   \
    F(nanosleep)                                                               \
    F(mremap)                                                                  \
    F(setresuid)                                                               \
    F(getresuid)                                                               \
    F(vm86)                                                                    \
    F(setresgid)                                                               \
    F(getresgid)                                                               \
    F(prctl)                                                                   \
    F(rt_sigreturn)                                                            \
    F(rt_sigaction)                                                            \
    F(rt_sigprocmask)                                                          \
    F(rt_sigpending)                                                           \
    F(rt_sigtimedwait)                                                         \
    F(rt_sigqueueinfo)                                                         \
    F(rt_sigsuspend)                                                           \
    F(pread64)                                                                 \
    F(pwrite64)                                                                \
    F(chown)                                                                   \
    F(capget)                                                                  \
    F(capset)                                                                  \
    F(sigaltstack)                                                             \
    F(sendfile)                                                                \
    F(vfork)                                                                   \
    F(ugetrlimit)                                                              \
    F(truncate64)                                                              \
    F(ftruncate64)                                                             \
    F(stat64)                                                                  \
    F(lstat64)                                                                 \
    F(fstat64)                                                                 \
    F(lchown32)                                                                \
    F(getuid32)                                                                \
    F(getgid32)                                                                \
    F(geteuid32)                                                               \
    F(getegid32)                                                               \
    F(setreuid32)                                                              \
    F(setregid32)                                                              \
    F(getgroups32)                                                             \
    F(setgroups32)                                                             \
    F(fchown32)                                                                \
    F(setresuid32)                                                             \
    F(getresuid32)                                                             \
    F(setresgid32)                                                             \
    F(getresgid32)                                                             \
    F(chown32)                                                                 \
    F(setuid32)                                                                \
    F(setgid32)                                                                \
    F(setfsuid32)                                                              \
    F(setfsgid32)                                                              \
    F(pivot_root)                                                              \
    F(mincore)                                                                 \
    F(madvise)                                                                 \
    F(getdents64)                                                              \
    F(fcntl64)                                                                 \
    F(readahead)                                                               \
    F(setxattr)                                                                \
    F(lsetxattr)                                                               \
    F(fsetxattr)                                                               \
    F(getxattr)                                                                \
    F(lgetxattr)                                                               \
    F(fgetxattr)                                                               \
    F(listxattr)                                                               \
    F(llistxattr)                                                              \
    F(flistxattr)                                                              \
    F(removexattr)                                                             \
    F(lremovexattr)                                                            \
    F(fremovexattr)                                                            \
    F(tkill)                                                                   \
    F(sendfile64)                                                              \
    F(futex)                                                                   \
    F(sched_setaffinity)                                                       \
    F(sched_getaffinity)                                                       \
    F(io_setup)                                                                \
    F(io_destroy)                                                              \
    F(io_getevents)                                                            \
    F(io_submit)                                                               \
    F(io_cancel)                                                               \
    F(fadvise64)                                                               \
    F(lookup_dcookie)                                                          \
    F(epoll_create)                                                            \
    F(epoll_ctl)                                                               \
    F(epoll_wait)                                                              \
    F(remap_file_pages)                                                        \
    F(set_tid_address)                                                         \
    F(timer_create)                                                            \
    F(timer_settime)                                                           \
    F(timer_gettime)                                                           \
    F(timer_getoverrun)                                                        \
    F(timer_delete)                                                            \
    F(clock_settime)                                                           \
    F(clock_gettime)                                                           \
    F(clock_getres)                                                            \
    F(clock_nanosleep)                                                         \
    F(statfs64)                                                                \
    F(fstatfs64)                                                               \
    F(tgkill)                                                                  \
    F(utimes)                                                                  \
    F(fadvise64_64)                                                            \
    F(mbind)                                                                   \
    F(get_mempolicy)                                                           \
    F(set_mempolicy)                                                           \
    F(mq_open)                                                                 \
    F(mq_unlink)                                                               \
    F(mq_timedsend)                                                            \
    F(mq_timedreceive)                                                         \
    F(mq_notify)                                                               \
    F(mq_getsetattr)                                                           \
    F(kexec_load)                                                              \
    F(waitid)                                                                  \
    F(add_key)                                                                 \
    F(request_key)                                                             \
    F(keyctl)                                                                  \
    F(ioprio_set)                                                              \
    F(ioprio_get)                                                              \
    F(inotify_init)                                                            \
    F(inotify_add_watch)                                                       \
    F(inotify_rm_watch)                                                        \
    F(migrate_pages)                                                           \
    F(openat)                                                                  \
    F(mkdirat)                                                                 \
    F(mknodat)                                                                 \
    F(fchownat)                                                                \
    F(futimesat)                                                               \
    F(fstatat64)                                                               \
    F(unlinkat)                                                                \
    F(renameat)                                                                \
    F(linkat)                                                                  \
    F(symlinkat)                                                               \
    F(readlinkat)                                                              \
    F(fchmodat)                                                                \
    F(faccessat)                                                               \
    F(pselect6)                                                                \
    F(ppoll)                                                                   \
    F(unshare)                                                                 \
    F(set_robust_list)                                                         \
    F(get_robust_list)                                                         \
    F(splice)                                                                  \
    F(sync_file_range)                                                         \
    F(tee)                                                                     \
    F(vmsplice)                                                                \
    F(move_pages)                                                              \
    F(getcpu)                                                                  \
    F(epoll_pwait)                                                             \
    F(utimensat)                                                               \
    F(signalfd)                                                                \
    F(timerfd_create)                                                          \
    F(eventfd)                                                                 \
    F(fallocate)                                                               \
    F(timerfd_settime)                                                         \
    F(timerfd_gettime)                                                         \
    F(signalfd4)                                                               \
    F(eventfd2)                                                                \
    F(epoll_create1)                                                           \
    F(dup3)                                                                    \
    F(pipe2)                                                                   \
    F(inotify_init1)                                                           \
    F(preadv)                                                                  \
    F(pwritev)                                                                 \
    F(rt_tgsigqueueinfo)                                                       \
    F(perf_event_open)                                                         \
    F(recvmmsg)                                                                \
    F(fanotify_init)                                                           \
    F(fanotify_mark)                                                           \
    F(prlimit64)                                                               \
    F(name_to_handle_at)                                                       \
    F(open_by_handle_at)                                                       \
    F(clock_adjtime)                                                           \
    F(syncfs)                                                                  \
    F(sendmmsg)                                                                \
    F(setns)                                                                   \
    F(process_vm_readv)                                                        \
    F(process_vm_writev)                                                       \
    F(kcmp)                                                                    \
    F(finit_module)                                                            \
    F(sched_setattr)                                                           \
    F(sched_getattr)                                                           \
    F(renameat2)                                                               \
    F(seccomp)                                                                 \
    F(getrandom)                                                               \
    F(memfd_create)                                                            \
    F(bpf)                                                                     \
    F(execveat)                                                                \
    F(socketpair)                                                              \
    F(getsockopt)                                                              \
    F(setsockopt)                                                              \
    F(getsockname)                                                             \
    F(getpeername)                                                             \
    F(sendto)                                                                  \
    F(sendmsg)                                                                 \
    F(recvfrom)                                                                \
    F(recvmsg)                                                                 \
    F(userfaultfd)                                                             \
    F(membarrier)                                                              \
    F(mlock2)                                                                  \
    F(copy_file_range)                                                         \
    F(preadv2)                                                                 \
    F(pwritev2)                                                                \
    F(pkey_mprotect)                                                           \
    F(pkey_alloc)                                                              \
    F(pkey_free)                                                               \
    F(statx)                                                                   \
    F(arch_prctl)                                                              \
    F(io_pgetevents)                                                           \
    F(rseq)                                                                    \
    F(semget)                                                                  \
    F(semctl)                                                                  \
    F(shmget)                                                                  \
    F(shmctl)                                                                  \
    F(shmat)                                                                   \
    F(shmdt)                                                                   \
    F(msgget)                                                                  \
    F(msgsnd)                                                                  \
    F(msgrcv)                                                                  \
    F(msgctl)                                                                  \
    F(clock_settime64)                                                         \
    F(clock_adjtime64)                                                         \
    F(clock_getres_time64)                                                     \
    F(timer_gettime64)                                                         \
    F(timer_settime64)                                                         \
    F(timerfd_gettime64)                                                       \
    F(timerfd_settime64)                                                       \
    F(utimensat_time64)                                                        \
    F(pselect6_time64)                                                         \
    F(ppoll_time64)                                                            \
    F(io_pgetevents_time64)                                                    \
    F(recvmmsg_time64)                                                         \
    F(mq_timedsend_time64)                                                     \
    F(mq_timedreceive_time64)                                                  \
    F(semtimedop_time64)                                                       \
    F(rt_sigtimedwait_time64)                                                  \
    F(futex_time64)                                                            \
    F(sched_rr_get_interval_time64)                                            \
    F(pidfd_send_signal)                                                       \
    F(io_uring_setup)                                                          \
    F(io_uring_enter)                                                          \
    F(io_uring_register)
