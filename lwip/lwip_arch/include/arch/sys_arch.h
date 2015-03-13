/* 
 * lwip/arch/sys_arch.h
 *
 * Arch-specific semaphores and mailboxes for lwIP running on mini-os 
 *
 * Tim Deegan <Tim.Deegan@eu.citrix.net>, July 2007
 */

// Edited: Charles Gruenwald III (Converted to fos types)

#ifndef __LWIP_ARCH_SYS_ARCH_H__
#define __LWIP_ARCH_SYS_ARCH_H__

#include <stdlib.h>

//#include <messaging/messaging.h>
//#include <utilities/semaphore.h>

#define SYS_SEM_NULL ((sys_sem_t) NULL)
#define SYS_MBOX_NULL ((sys_mbox_t) 0)

#if !NO_SYS

typedef FosSemaphore *sys_sem_t;

typedef struct
{
    FosMailbox *mbox;
    FosMailboxCapability cap;
    FosMailboxAlias alias;
} sys_mbox_;

typedef sys_mbox_ *sys_mbox_t;

typedef struct thread *sys_thread_t;

typedef unsigned long sys_prot_t;

#endif

#endif /*__LWIP_ARCH_SYS_ARCH_H__ */
