#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <synch.h>
#include <mips/trapframe.h>
#include <array.h>
#include <test.h>


/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */


#if OPT_A2
pid_t
fork(struct trapframe *trap, pid_t *retval){
    struct proc *newproc = proc_create_runprogram(curproc->p_name);
    if(newproc == NULL){
        return ENOMEM;
    }
    as_copy(curproc_getas(), &(newproc->p_addrspace));
    if(newproc->p_addrspace == NULL){
        proc_destroy(newproc);
        return ENOMEM;
    }
    struct trapframe *newtrap = kmalloc(sizeof(struct trapframe));
    if(newtrap == NULL){
        proc_destroy(newproc);
        return ENOMEM;
    }
    memcpy(newtrap, trap, sizeof(struct trapframe));

    int newthread = thread_fork(curthread->t_name, newproc, &enter_forked_process, newtrap, 0);
    if(newthread != 0){
        kfree(newtrap);
        proc_destroy(newproc);
        return ENOMEM;
    }
    *retval = newproc->p_id;
    array_add(curproc->children, newproc, NULL);
    lock_acquire(newproc->exit_lock);
    return 0;
}
#endif





void sys__exit(int exitcode) {

    struct addrspace *as;
    struct proc *p = curproc;
    /* for now, just include this to keep the compiler from complaining about
       an unused variable */
    (void)exitcode;

    DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

    KASSERT(curproc->p_addrspace != NULL);

    
#if OPT_A2
    for(unsigned int a = array_num(p->children);a > 0;a--){
        struct proc *temp = array_get(p->children, a-1);
        lock_release(temp->exit_lock);
        array_remove(p->children, a-1);
    }
    p->if_exit = true;
    p->exitnum = _MKWAIT_EXIT(exitcode);
    lock_acquire(p->check_children);
    cv_broadcast(p->child_cv, p->check_children);
    lock_release(p->check_children);
    lock_acquire(p->exit_lock);
    lock_release(p->exit_lock);
#endif


    as_deactivate();
    /*
     * clear p_addrspace before calling as_destroy. Otherwise if
     * as_destroy sleeps (which is quite possible) when we
     * come back we'll be calling as_activate on a
     * half-destroyed address space. This tends to be
     * messily fatal.
     */
    as = curproc_setas(NULL);
    as_destroy(as);

    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_remthread(curthread);

    /* if this is the last user process in the system, proc_destroy()
       will wake up the kernel menu thread */
    proc_destroy(p);

    thread_exit();
    /* thread_exit() does not return, so we should never get here */
    panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
    int
sys_getpid(pid_t *retval)
{
#if OPT_A2
    *retval=curproc->p_id;
    return 0;
#else
    /* for now, this is just a stub that always returns a PID of 1 */
    /* you need to fix this to make it work properly */
    *retval = 1;
    return(0);
#endif
}

/* stub handler for waitpid() system call                */

    int
sys_waitpid(pid_t pid,
        userptr_t status,
        int options,
        pid_t *retval)
{
    int exitstatus;
    int result;


#if OPT_A2
    struct proc* check_proc=find_proc_with_id(pid);
    lock_acquire(check_proc->check_children);
    while(check_proc->if_exit != true){
        cv_wait(check_proc->child_cv,check_proc->check_children);
    }
    lock_release(check_proc->check_children);
    exitstatus = check_proc->exitnum;
#endif


    /* this is just a stub implementation that always reports an
       exit status of 0, regardless of the actual exit status of
       the specified process.
       In fact, this will return 0 even if the specified process
       is still running, and even if it never existed in the first place.

       Fix this!
       */

    if (options != 0) {
        return(EINVAL);
    }
    result = copyout((void *)&exitstatus,status,sizeof(int));
    if (result) {
        return(result);
    }
    *retval = pid;
    return(0);
}




#if OPT_A2
int
execv(const char* program, char** args, pid_t *retval){
    unsigned long arg_num = 0;
    size_t len = strlen(program) + 1;
    size_t finallen = 0;
    while(args[arg_num] != NULL){
        finallen=finallen + strlen(args[arg_num]) + 1;
        arg_num++;
    }
    char copyedprogram[len];
    int if_copyed = 0;
    if_copyed = copyinstr((userptr_t)program, copyedprogram, len, NULL);
    char copyedargs[finallen];
    char *newargs[arg_num];
    int copyed = 0;
    unsigned long check = 0;
    while(check < arg_num){
        size_t temp;
        len = strlen(args[check]) + 1;
        if_copyed = copyinstr((userptr_t)args[check], (copyedargs+copyed), len, &temp);
        newargs[check] = copyedargs + copyed;
        copyed = copyed + temp;
        check++;
    }
    int result = runprogram(copyedprogram,newargs,arg_num);
    *retval = result;
    return 1;
}
#endif
