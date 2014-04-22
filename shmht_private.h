#ifndef __HASHTABLE_PRIVATE_CWC22_H__
#define __HASHTABLE_PRIVATE_CWC22_H__

#include "shmht.h"


//Max size of a key = > By default 512 bytes.
#define MAX_KEY_SIZE 512
/*****************************************************************************/

struct entry
{
	//Marks if the entry is used or not.
	int used;
	//Store the key in a char array, later, transform to a void *, and the 
	//compare function will be who treat it as it is. 
	// BE CAREFULL with keys with longer sizes, the behaviour is not defined!!.
	char k[MAX_KEY_SIZE];
	//key_size
	size_t key_size;
	//Hash of the key.
	unsigned int h;
	//Offset of the next. (Must be in collisions)
	unsigned int next;
	//offset of the bucket where the entry is stored on.
	int bucket;
	//The size of the stored in the bucket. (This is to allow storing variable size
	//items, maximun, the size of the bucket). We only copy to the destiny, the
	//stored size, not all the bucket. [optimization]
	int bucket_stored_size;
	//Position where the entry is stored on. In the entries and in the colisions.
	int position;
	//Seconds from epoch. This is the creation time.
	//We use this value for deleting the older values, we use seconds, beacause
	//is an aproximate cleaning (designed for cache pourposes).
	long sec;
};

//Struct only with the flag of used / not.
struct bucket
{
	int used;
};



struct internal_hashtable
{
	unsigned int tablelength;
	unsigned int registry_max_size;
	unsigned int semaphore;
	unsigned int shmid;
	unsigned int entrycount;
	unsigned int primeindex;
};


struct shmht
{
	//Those values are updated at creation time, so, they MUST be pointers.
	//Are process related values, so to each process return his pointers.
	void *internal_ht;
	void *entrypoint;
	void *collisionentries;
	void *bucketmarket;

	// Functions related to the data type stored.
	unsigned int (*hashfn) (void *k);
	int (*eqfn) (void *k1, void *k2);

};

/*****************************************************************************/
static unsigned int hash (struct shmht *h, void *k);

/*****************************************************************************/
/* indexFor */
static inline unsigned int
indexFor (unsigned int tablelength, unsigned int hashvalue)
{
	return (hashvalue % tablelength);
};


/*****************************************************************************/

#endif /* __HASHTABLE_PRIVATE_CWC22_H__ */
