/*------------------------------------------------------------------------*/
/* Sample Code of OS Dependent Functions for FatFs                        */
/* (C)ChaN, 2017                                                          */
/* Modified for aCoral OS by User, 2023                                   */
/*------------------------------------------------------------------------*/


#include "ff.h"
#include "acoral.h"
#include "sem.h"


/*------------------------------------------------------------------------*/
/* Get system time                                                        */
/*------------------------------------------------------------------------*/

DWORD get_fattime(void)
{
    // 返回一个固定的时间 (2023/01/01 00:00:00)
    // 格式: bit31-25: 年(0-127 从 1980 年开始), bit24-21: 月(1-12), bit20-16: 日(1-31)
    // bit15-11: 时(0-23), bit10-5: 分(0-59), bit4-0: 秒/2(0-29)
    return ((DWORD)(2023 - 1980) << 25) |
           ((DWORD)1 << 21) |
           ((DWORD)1 << 16) |
           ((DWORD)0 << 11) |
           ((DWORD)0 << 5) |
           ((DWORD)0 >> 1);
}


#if FF_USE_LFN == 3	/* Dynamic memory allocation */

/*------------------------------------------------------------------------*/
/* Allocate a memory block                                                */
/*------------------------------------------------------------------------*/

void* ff_memalloc (	/* Returns pointer to the allocated memory block (null on not enough core) */
	UINT msize		/* Number of bytes to allocate */
)
{
	return malloc(msize);	/* Allocate a new memory block with POSIX API */
}


/*------------------------------------------------------------------------*/
/* Free a memory block                                                    */
/*------------------------------------------------------------------------*/

void ff_memfree (
	void* mblock	/* Pointer to the memory block to free (nothing to do for null) */
)
{
	free(mblock);	/* Free the memory block with POSIX API */
}

#endif



#if FF_FS_REENTRANT	/* Mutal exclusion */

/*------------------------------------------------------------------------*/
/* Create a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount() function to create a new
/  synchronization object for the volume, such as semaphore and mutex.
/  When a 0 is returned, the f_mount() function fails with FR_INT_ERR.
*/

int ff_cre_syncobj (	/* 1:Function succeeded, 0:Could not create the sync object */
	BYTE vol,			/* Corresponding volume (logical drive number) */
	FF_SYNC_t* sobj		/* Pointer to return the created sync object */
)
{
	/* aCoral OS */
    *sobj = acoral_sem_create(1);  // 创建一个初始值为1的信号量
    return (*sobj != NULL) ? 1 : 0;
}


/*------------------------------------------------------------------------*/
/* Delete a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
/* This function is called in f_mount() function to delete a synchronization
/  object that created with ff_cre_syncobj() function. When a 0 is returned,
/  the f_mount() function fails with FR_INT_ERR.
*/

int ff_del_syncobj (	/* 1:Function succeeded, 0:Could not delete due to an error */
	FF_SYNC_t sobj		/* Sync object tied to the logical drive to be deleted */
)
{
	/* aCoral OS */
    acoral_sem_del(sobj);
    return 1;
}


/*------------------------------------------------------------------------*/
/* Request Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on entering file functions to lock the volume.
/  When a 0 is returned, the file function fails with FR_TIMEOUT.
*/

int ff_req_grant (	/* 1:Got a grant to access the volume, 0:Could not get a grant */
	FF_SYNC_t sobj	/* Sync object to wait */
)
{
	/* aCoral OS */
    return (acoral_sem_pend(sobj, FF_FS_TIMEOUT) == SEM_SUCCED) ? 1 : 0;
}


/*------------------------------------------------------------------------*/
/* Release Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
/* This function is called on leaving file functions to unlock the volume.
*/

void ff_rel_grant (
	FF_SYNC_t sobj	/* Sync object to be signaled */
)
{
	/* aCoral OS */
    acoral_sem_post(sobj);
}

#endif

