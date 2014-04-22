/*
 * This is a R/W lock implementation using the SYS semaphores.
 * Based on http://www.experts-exchange.com/Programming/Languages/C/Q_23939132.html
 */


#ifndef __HASHTABLE_SEM__
#define __HASHTABLE_SEM__

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define SEM_READER 0
#define SEM_WRITER 1

struct sembuf read_start[] =
	{ {SEM_READER, 1, SEM_UNDO}, {SEM_WRITER, 0, SEM_UNDO} };
struct sembuf read_end[] = { {SEM_READER, -1, SEM_UNDO} };

struct sembuf write_start1[] = { {SEM_WRITER, 1, SEM_UNDO} };
struct sembuf write_start2[] =
	{ {SEM_READER, 0, SEM_UNDO}, {SEM_READER, 1, SEM_UNDO} };
struct sembuf write_fail_end[] = { {SEM_WRITER, -1, SEM_UNDO} };
struct sembuf write_end[] =
	{ {SEM_READER, -1, SEM_UNDO}, {SEM_WRITER, -1, SEM_UNDO} };

#define SEMOP(semid,tbl,exc) { if(0>semop (semid, tbl, sizeof(tbl)/sizeof(struct sembuf) )){perror("semop: ");return exc;}}

//Necessary stuff for locking.
int
write_end_proc (int semid)
{
	SEMOP (semid, write_fail_end, 0);
	return -1;
}

#define READ_LOCK(semid) SEMOP(semid,read_start,-1)
#define READ_UNLOCK(semid) SEMOP(semid,read_end,-1)

#define WRITE_LOCK_READERS(semid) (SEMOP(semid,write_start1,-1))
#define WRITE_LOCK_TO_WRITE(semid) (SEMOP(semid,write_start2,write_end_proc(semid)))

//This macro returns a 0 different value if something goes wrong with the locking.
#define WRITE_LOCK(semid) (WRITE_LOCK_READERS(semid) && WRITE_LOCK_TO_WRITE(semid))

#define WRITE_UNLOCK(semid) SEMOP(semid,write_end, -1)

int
read_lock (int semid)
{
	READ_LOCK (semid);
	return 0;
}

int
read_unlock (int semid)
{
	READ_UNLOCK (semid);
	return 0;
}

int
write_lock (int semid)
{
	WRITE_LOCK_READERS (semid);
	WRITE_LOCK_TO_WRITE (semid);
	return 0;
}

int
write_unlock (int semid)
{
	WRITE_UNLOCK (semid);
	return 0;
}

#endif // __HASHTABLE_SEM__
