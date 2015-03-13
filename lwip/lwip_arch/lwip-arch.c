/* 
 * lwip-arch.c
 *
 * Arch-specific semaphores and mailboxes for lwIP running on mini-os 
 *
 * Tim Deegan <Tim.Deegan@eu.citrix.net>, July 2007
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <lwip/sys.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>
#include <arch/perf.h>

//#include <utilities/time_arith.h>

#if !NO_SYS

/* Is called to initialize the sys_arch layer */
void sys_init()
{
}

/* Creates and returns a new semaphore. The "count" argument specifies
 * the initial state of the semaphore. */
sys_sem_t sys_sem_new(u8_t count)
{
    printf("sem new not tested\n");
    FosSemaphore *sem = malloc(sizeof(sem));
    fosSemInit(sem,count);
    return sem;
}

/* Deallocates a semaphore. */
void sys_sem_free(sys_sem_t sem)
{
    printf("sem free not tested\n");
    free(sem);
}

/* Signals a semaphore. */
void sys_sem_signal(sys_sem_t sem)
{
    printf("sem signal not implemented\n");
    fosSemSignal(sem);
}

/* Blocks the thread while waiting for the semaphore to be
 * signaled. If the "timeout" argument is non-zero, the thread should
 * only be blocked for the specified time (measured in
 * milliseconds).
 * 
 * If the timeout argument is non-zero, the return value is the number of
 * milliseconds spent waiting for the semaphore to be signaled. If the
 * semaphore wasn't signaled within the specified time, the return value is
 * SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
 * (i.e., it was already signaled), the function may return zero. */
u32_t sys_arch_sem_wait(sys_sem_t sem, u32_t timeout)
{
    printf("sem wait -- timeout: %d\n", timeout);
    struct timeval timeout_tv;
    struct timeval start;
    struct timeval now;
    __do_fos_syscall(SYSCALL_GETTIMEOFDAY, &start);
    timeout_tv = start;

    timeout_tv.tv_usec += (timeout * 1000);

    //overflow usec...
    if(timeout_tv.tv_usec > 1000000000)
    {
        timeout_tv.tv_sec += 1;
        timeout_tv.tv_usec %= 1000000000;
    }

    while(true)
    {
        __do_fos_syscall(SYSCALL_GETTIMEOFDAY, &now);

        fosLockAcquire(&sem->lock);
        if(sem->count > 0)
        {
            sem->count--;
            fosLockRelease(&sem->lock);
            break;
        }
        fosLockRelease(&sem->lock);

        if(timeout_tv.tv_sec < now.tv_sec || 
                (timeout_tv.tv_sec == now.tv_sec && timeout_tv.tv_usec < now.tv_usec)  )
        {
//            printf("semaphore timed out\n");
            if(timeout)
                return SYS_ARCH_TIMEOUT;
        }

        //
    }


    //calculate the total elapsed milliseconds
    struct timeval elapsed;
    if(timeval_subtract(&elapsed, &now, &start))
       printf("ERROR - got negative timeout in sem wait\n");

   //Convert the timeval into a ms elapsed
   return elapsed.tv_sec * 1000 + elapsed.tv_usec / 1000;
}

/* Creates an empty mailbox. */
sys_mbox_t sys_mbox_new(int size)
{
    FosMailbox *netstack_mailbox;
    FosMailboxCapability netstack_mailbox_capability;
    FosMailboxAlias netstack_mailbox_canonical_alias;
    FosMailboxAliasCapability netstack_mailbox_alias_capability;

    printf("Creating a new mailbox...\n");
    // first allocate the mailbox
    if (fosMailboxCreate(&netstack_mailbox, ((64)*(1024))) != FOS_MAILBOX_STATUS_OK)
    {
        fosError("mailbox creation problem 6");
    }

    fosMailboxGetCanonicalAlias(&netstack_mailbox_canonical_alias, netstack_mailbox);

    // create a capability for our input port which will be the capability that we share with 'init' to share with most of the world
    if (fosMailboxCapabilityCreate(&netstack_mailbox_capability, netstack_mailbox, FOS_FLAG_NONE) != FOS_MAILBOX_STATUS_OK)
    {
        fosError("default capability cannot be created");
    }

    sys_mbox_t mbox;
    mbox = malloc(sizeof(sys_mbox_));
    mbox->mbox = netstack_mailbox;
    mbox->alias = netstack_mailbox_canonical_alias;
    mbox->cap = netstack_mailbox_capability;

    return mbox;
}

/* Deallocates a mailbox. If there are messages still present in the
 * mailbox when the mailbox is deallocated, it is an indication of a
 * programming error in lwIP and the developer should be notified. */
void sys_mbox_free(sys_mbox_t mbox)
{
    printf("mbox free not implemented\n");
#if 0
    ASSERT(mbox->reader == mbox->writer);
    xfree(mbox->messages);
    xfree(mbox);
#endif
}

/* Posts the "msg" to the mailbox. */
void sys_mbox_post(sys_mbox_t mbox, void *msg)
{

    printf("sys_mbox_post called\n");
    //TODO: find the size of these messages...
    //we may need a deeper copy if these are coming
    //from a different application
    fosMailboxSend(&mbox->alias, mbox->cap, &msg, sizeof(char*));
}

/* Try to post the "msg" to the mailbox. */
err_t sys_mbox_trypost(sys_mbox_t mbox, void *msg)
{
    printf("sys_mbox_trypost called\n");
    //TODO: find the size of these messages...
    //we may need a deeper copy if these are coming
    //from a different application
    fosMailboxSend(&mbox->alias, mbox->cap, &msg, sizeof(char*));
}

extern int netstack_loop();

/* Blocks the thread until a message arrives in the mailbox, but does
 * not block the thread longer than "timeout" milliseconds (similar to
 * the sys_arch_sem_wait() function). The "msg" argument is a result
 * parameter that is set by the function (i.e., by doing "*msg =
 * ptr"). The "msg" parameter maybe NULL to indicate that the message
 * should be dropped.
 *
 * The return values are the same as for the sys_arch_sem_wait() function:
 * Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
 * timeout. */
u32_t sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t timeout)
{
    struct timeval start;
    struct timeval now;
    struct timeval timeout_tv;
    __do_fos_syscall(SYSCALL_GETTIMEOFDAY, &start);
    timeout_tv = start;

    timeout_tv.tv_usec += (timeout * 1000);

    //overflow usec...
    if(timeout_tv.tv_usec > 1000000000)
    {
        timeout_tv.tv_sec += 1;
        timeout_tv.tv_usec %= 1000000000;
    }

    while(1)
    {
        //see if we timed out
        __do_fos_syscall(SYSCALL_GETTIMEOFDAY, &now);

        if(timeout_tv.tv_sec < now.tv_sec || 
                (timeout_tv.tv_sec == now.tv_sec && timeout_tv.tv_usec < now.tv_usec)  )
                return SYS_ARCH_TIMEOUT;


        // call the netstack loop to process other events 
        // perhaps this should be in a separate thread
        netstack_loop();

        // look for messages on the mailbox provided
        char * received_message;
        size_t received_size;
        FosStatus receive_error;
        receive_error = fosMailboxReceive((void**) &received_message, &received_size, mbox->mbox);
        if ((receive_error == FOS_MAILBOX_STATUS_ALLOCATION_ERROR) || (receive_error == FOS_MAILBOX_STATUS_INVALID_MAILBOX_ERROR))
        {
            fosError("unrecoverable mailbox recieve error");
        }
        else if (receive_error == FOS_MAILBOX_STATUS_EMPTY_ERROR)
        {
            //usleep(100);
        }
        else if(receive_error == FOS_MAILBOX_STATUS_OK)
        {
            //the message is simply an address do a shallow copy
            *msg = ((int *)received_message)[0];

            fosMailboxBufferFree(mbox->mbox, (char *)received_message);
            break;
        }
    }

    //calculate the total elapsed milliseconds
    struct timeval elapsed;
    timeval_subtract(&elapsed, &now, &start);

    //Convert the timeval into a ms elapsed
    return elapsed.tv_sec * 1000 + elapsed.tv_usec / 1000;
}

/* This is similar to sys_arch_mbox_fetch, however if a message is not
 * present in the mailbox, it immediately returns with the code
 * SYS_MBOX_EMPTY. On success 0 is returned.
 *
 * To allow for efficient implementations, this can be defined as a
 * function-like macro in sys_arch.h instead of a normal function. For
 * example, a naive implementation could be:
 *   #define sys_arch_mbox_tryfetch(mbox,msg) \
 *     sys_arch_mbox_fetch(mbox,msg,1)
 * although this would introduce unnecessary delays. */

u32_t sys_arch_mbox_tryfetch(sys_mbox_t mbox, void **msg) {

    //look for messages from the netlink, possibly we should just read from the mbox
    char * received_message;
    size_t received_size;
    FosStatus receive_error;

    receive_error = fosMailboxReceive((void**) &received_message, &received_size, mbox->mbox);
    if ((receive_error == FOS_MAILBOX_STATUS_ALLOCATION_ERROR) || (receive_error == FOS_MAILBOX_STATUS_INVALID_MAILBOX_ERROR))
    {
        fosError("unrecoverable mailbox recieve error");
    }
    else if (receive_error == FOS_MAILBOX_STATUS_EMPTY_ERROR)
    {
        return SYS_MBOX_EMPTY;
    }
    else if(receive_error == FOS_MAILBOX_STATUS_OK)
    {
        printf("message received from netstack/app, sending it up the stack.\n");
        *msg = malloc(received_size);
        memcpy(msg,received_message,received_size); //FIXME: can we avoid this copy by allowing the netstack to use the mailbox
        //buffer free mechanism

        fosMailboxBufferFree(mbox->mbox, (char *)received_message);
        return 0;
    }
    else
    {
        printf("unknown mailbox error in lwip-arch.\n");
    }
}

/* Returns a pointer to the per-thread sys_timeouts structure. In lwIP,
 * each thread has a list of timeouts which is repressented as a linked
 * list of sys_timeout structures. The sys_timeouts structure holds a
 * pointer to a linked list of timeouts. This function is called by
 * the lwIP timeout scheduler and must not return a NULL value. 
 *
 * In a single threadd sys_arch implementation, this function will
 * simply return a pointer to a global sys_timeouts variable stored in
 * the sys_arch module. */
struct sys_timeouts *sys_arch_timeouts(void) 
{
    static struct sys_timeouts timeout;
    return &timeout;
}

/* Starts a new thread with priority "prio" that will begin its execution in the
 * function "thread()". The "arg" argument will be passed as an argument to the
 * thread() function. The id of the new thread is returned. Both the id and
 * the priority are system dependent. */
static struct thread *lwip_thread;
sys_thread_t sys_thread_new(char *name, void (* thread)(void *arg), void *arg, int stacksize, int prio)
{
    printf("sys_thread_new not implemented\n");
    printf("was trying to create thread named: %s\n", name);
#if 0
    struct thread *t;
    if (stacksize > STACK_SIZE) {
	printk("Can't start lwIP thread: stack size %d is too large for our %d\n", stacksize, STACK_SIZE);
	do_exit();
    }
    lwip_thread = t = create_thread(name, thread, arg);
    return t;
#endif
}
#endif

/* This optional function does a "fast" critical region protection and returns
 * the previous protection level. This function is only called during very short
 * critical regions. An embedded system which supports ISR-based drivers might
 * want to implement this function by disabling interrupts. Task-based systems
 * might want to implement this by using a mutex or disabling tasking. This
 * function should support recursive calls from the same task or interrupt. In
 * other words, sys_arch_protect() could be called while already protected. In
 * that case the return value indicates that it is already protected.
 *
 * sys_arch_protect() is only required if your port is supporting an operating
 * system. */
sys_prot_t sys_arch_protect(void)
{
    unsigned long flags = 0;
//    printf("sys_arch_protect not implemented\n");
#if 0
    local_irq_save(flags);
#endif
    return flags;
}

/* This optional function does a "fast" set of critical region protection to the
 * value specified by pval. See the documentation for sys_arch_protect() for
 * more information. This function is only required if your port is supporting
 * an operating system. */
void sys_arch_unprotect(sys_prot_t pval)
{
//    printf("sys_arch_unprotect not implemented\n");
#if 0
    local_irq_restore(pval);
#endif
}

/* non-fatal, print a message. */
void lwip_printk(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("lwIP: ");
    vprintf(fmt, args);
    va_end(args);
}

/* fatal, print message and abandon execution. */
void lwip_die(char *fmt, ...)
{
    printf("die not implemented\n");
    va_list args;
    va_start(args, fmt);
    printf("lwIP assertion failed: ");
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

