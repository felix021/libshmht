#include "shmht.h"
#include "shmht_sem.h"
#include "shmht_private.h"
#include "shmht_debug.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

/*
Credit for primes table: Aaron Krowne
 http://br.endernet.org/~akrowne/
 http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
*/
static const unsigned int primes[] = {
	53, 97, 193, 389,
	769, 1543, 3079, 6151,
	12289, 24593, 49157, 98317,
	196613, 393241, 786433, 1572869,
	3145739, 6291469, 12582917, 25165843,
	50331653, 100663319, 201326611, 402653189,
	805306457, 1610612741
};

const unsigned int prime_table_length = sizeof (primes) / sizeof (primes[0]);
const float max_load_factor = 0.65;

/*Necessary union for the semaphore*/
union semun
{
	int val;					/* value for SETVAL */
	struct semid_ds *buf;		/* buffer for IPC_STAT, IPC_SET */
	unsigned short *array;		/* array for GETALL, SETALL */
	struct seminfo *__buf;		/* buffer for IPC_INFO */
};


/****************************************************/
struct shmht *
create_shmht (char *name,
				  unsigned int number,
				  size_t register_size,
				  unsigned int (*hashf) (void *), int (*eqf) (void *, void *))
{

	void *primary_pointer;
	struct shmht *h;
	int semaphore;
	int created = 0;
	unsigned int pindex, size = primes[0];
	//Stuff for the semaphore.
	union semun arg;
	arg.val = 0;

	//First create the key for shmget and semget.
	//Be careful the file must exist.
	key_t shm_sem_key = ftok (name, 1);
	if (shm_sem_key < 0) {
		perror ("ftok: ");
		return NULL;
	}

	/* Check requested hashtable isn't too large */
	if (number > (1u << 30))
		return NULL;
	/* Enforce size as prime */

	for (pindex = 0; pindex < prime_table_length; pindex++) {
		if (primes[pindex] > number) {
			size = primes[pindex];
			break;
		}
	}


	/*Calcule the necessary size for the hash table */
	//hashtable structure + 2* entry size + buckets.
	int all_ht_size =
		sizeof (struct internal_hashtable) +
		2 * sizeof (struct entry) * size + (sizeof (struct bucket) +
											register_size) * size;

	int id = shmget (shm_sem_key, all_ht_size, 0666);
	if (id < 0) {
		id = shmget (shm_sem_key, all_ht_size, IPC_CREAT | 0666);
		created = 1;
	}
	if (id < 0) {
		perror ("shmget: ");
		return NULL;
	}

	shmht_debug (("create_shmht: The shmem id is: %d\n The size is %d \n",
				  id, all_ht_size));

	primary_pointer = shmat (id, NULL, 0);
	h = (struct shmht *) malloc (sizeof (struct shmht));

	if (created) {
		shmht_debug (("create_shmht: As created, clean all the shm!\n"));
		bzero (primary_pointer, all_ht_size);
	}

	//Check if the shmat has worked.
	if (h == NULL)
		return h;

	//Allocate the semaphores (two semaphores for R/W lock).
	semaphore = semget (shm_sem_key, 2, 0666);
	if (semaphore < 0) {
		semaphore = semget (shm_sem_key, 2, IPC_CREAT | 0666);
		//Init the two values of the sem to 0 (unlocked).
		shmht_debug (("create_shmht: Init the semaphores to 0 (unlocked)\n"));
		if (semctl (semaphore, 0, SETVAL, arg) == -1) {
			perror ("semctl: ");
			return NULL;
		}
		if (semctl (semaphore, 1, SETVAL, arg) == -1) {
			perror ("semctl: ");
			return NULL;
		}
	}
	if (semaphore < 0) {
		perror ("semget: ");
		return NULL;
	}

	shmht_debug (("create_shmht: The id of the semaphore is: %d\n",
				  semaphore));

	//The created structure:
	//------------------------------------------------------------
	//| internal_hashtable | entries | colision entries | buckets |
	//------------------------------------------------------------
	//Entries point, use the void* to do the pointer arithmetic:
	h->internal_ht = primary_pointer;
	h->entrypoint = h->internal_ht + sizeof (struct internal_hashtable);
	//Collision entries:
	h->collisionentries = h->entrypoint + sizeof (struct entry) * size;
	//Bucket entries:
	h->bucketmarket = h->entrypoint + 2 * sizeof (struct entry) * size;


	//Store the necessary values:
	struct internal_hashtable *iht = h->internal_ht;

	if (created) {
		iht->semaphore = semaphore;
		iht->shmid = id;
	}

	//The register_size
	if (!created)
		assert
			("Logical Error: initiated with different length the hash table"
			 || iht->registry_max_size != register_size);

	iht->registry_max_size = register_size;
	//The number of registers
	if (!created)
		assert
			("Logical Error: initiated with different length the hash table"
			 || iht->tablelength != size);
	iht->tablelength = size;
	//Index in the prime array.
	iht->primeindex = pindex;
	//Number of entries.
	if (created)
		iht->entrycount = 0;
	//Its possible to update in runtime the hash functions of the HT.
	//hash function.
	h->hashfn = hashf;
	//equal funcion.
	h->eqfn = eqf;
	return h;
}								//create_shmht

/*****************************************************************************/
static unsigned int
hash (struct shmht *h, void *k)
{
	/* Aim to protect against poor hash functions by adding logic here
	 * - logic taken from java 1.4 hashtable source */
	unsigned int i = h->hashfn (k);
	i += ~(i << 9);
	i ^= ((i >> 14) | (i << 18));	/* >>> */
	i += (i << 4);
	i ^= ((i >> 10) | (i << 22));	/* >>> */
	return i;
}


/*****************************************************************************/
int
shmht_count (struct shmht *h)
{
	int value = -1;
	struct internal_hashtable *iht = h->internal_ht;
	READ_LOCK (iht->semaphore);
	value = iht->entrycount;
	READ_UNLOCK (iht->semaphore);
	return value;
}								// hashtable_count


/****************************************************************************/

//This function looks for free colision entries in the hash table.
static int
locate_free_colision_entry (struct shmht *h)
{
	int i;
	struct internal_hashtable *iht = h->internal_ht;
	for (i = 0; i < iht->tablelength; i++) {
		struct entry *aux = h->collisionentries + (i * sizeof (struct entry));
		if (!aux->used)
			return i;
	}
	return -1;
}								// locate_free_colision_entry


/*****************************************************************************/

//This function looks for free buckets in the shm.
static int
locate_free_bucket (struct shmht *h)
{
	int i;
	struct internal_hashtable *iht = h->internal_ht;
	for (i = 0; i < iht->tablelength; i++) {
		struct bucket *aux =
			h->bucketmarket +
			(i * (sizeof (struct bucket) + iht->registry_max_size));
		if (!aux->used)
			return i;
	}
	return -1;
}								// locate_free_bucket

/*****************************************************************************/
int
shmht_insert (struct shmht *h, void *k, size_t key_size,
				  void *v, size_t value_size)
{

	//first acquire the lock.
	//If it fails return -ECANCELED.
	struct internal_hashtable *iht = h->internal_ht;
	if (write_lock (iht->semaphore) < 0)
		return -ECANCELED;

	int index = -1;
	unsigned int key_hash;
	struct timeval tv;

	if (value_size > iht->registry_max_size) {
		write_unlock (iht->semaphore);
		return -EINVAL;
	}

	if (key_size > MAX_KEY_SIZE) {
		write_unlock (iht->semaphore);
		return -EINVAL;
	}

	//Test if we have reached the max size of the HT. This is FIXED.
	if (iht->tablelength <= iht->entrycount) {
		write_unlock (iht->semaphore);
		return -1;
	}

	//By default if there is size, should be free buckets, but check is almost free.
	index = locate_free_bucket (h);
	if (index < 0) {
		write_unlock (iht->semaphore);
		return -1;
	}

	//Get the seconds from epoch:
	gettimeofday (&tv, NULL);

	//Set to used.
	struct bucket *bucket_ptr =
		h->bucketmarket +
		(index * (sizeof (struct bucket) + iht->registry_max_size));
	bucket_ptr->used = 1;
	//Copy the value in the bucket.
	memcpy (h->bucketmarket +
			(index * (sizeof (struct bucket) + iht->registry_max_size)) +
			sizeof (struct bucket)
			, v, value_size);
	shmht_debug (("shmht_insert: Located free bucket in %d\n", index));
	key_hash = hash (h, k);
	int entryIndex = indexFor (iht->tablelength, key_hash);
	shmht_debug (("shmht_insert: Generated Entry Index: %d \n",
				  entryIndex));
	struct entry *index_Entry =
		h->entrypoint + (entryIndex * sizeof (struct entry));

	if (!index_Entry->used) {
		//!Colision
		index_Entry->used = 1;

		memcpy (index_Entry->k, k, key_size);
		index_Entry->key_size = key_size;
		index_Entry->h = key_hash;
		index_Entry->next = -1;
		index_Entry->bucket = index;
		index_Entry->position = entryIndex;
		index_Entry->bucket_stored_size = value_size;
		index_Entry->sec = tv.tv_sec;

	}
	else {
		//Colision
		shmht_debug (("shmht_insert: Collision in the entry %d \n",
					  entryIndex));
		int colision_index = locate_free_colision_entry (h);

		//Paranoid check.
		assert (colision_index == -1
				|| "Logical Error: Not free colision entries");
		shmht_debug (("shmht_insert: Located free colision entry : %d\n",
					  colision_index));
		struct entry *colision_Entry =
			h->collisionentries + (colision_index * sizeof (struct entry));
		//Paranoid check.
		assert (colision_Entry->used
				||
				"Logical Error: locate_free_colision_entry returns a used entry!");
		colision_Entry->used = 1;
		memcpy (colision_Entry->k, k, key_size);
		colision_Entry->key_size = key_size;
		colision_Entry->h = key_hash;
		colision_Entry->next = -1;
		colision_Entry->position = colision_index;
		colision_Entry->bucket = index;
		colision_Entry->bucket_stored_size = value_size;
		colision_Entry->sec = tv.tv_sec;
		//Look for the previous one.
		if (index_Entry->next == -1) {
			//There are not more colisions.
			index_Entry->next = colision_index;
			colision_Entry->next = -1;
		}
		else {
			//Look for the previous!
			struct entry *aux = index_Entry;
			while (aux->next != -1)
				aux =
					h->collisionentries + (aux->next * sizeof (struct entry));
			//Set all the stuff of entries:
			aux->next = colision_index;
			colision_Entry->next = -1;
		}
	}
	// Add 1 to the entrycount.
	iht->entrycount++;
	//unlock the write sem.
	write_unlock (iht->semaphore);

	return 1;
}								// shmht_insert

/*****************************************************************************/
//Compare two keys :D
//Return 0 if equal, 1 if not.

static int
compareBinaryKeys (size_t sk1, void *k1, size_t sk2, void *k2)
{
	//First compare key sizes:
	if (sk1 != sk2)
		return 1;

	return bcmp (k1, k2, sk1);
}								// compareBinaryKeys

/*****************************************************************************/
void *							/* returns the fist value associated with key */
shmht_search (struct shmht *h, void *k, size_t key_size,
				  size_t * returned_size)
{
	struct internal_hashtable *iht = h->internal_ht;
	if (read_lock (iht->semaphore) < 0)
		return NULL;
	void *retValue = NULL;
	struct entry *index_Entry;
	unsigned int hashvalue, index;

	//Calcule the hash
	hashvalue = hash (h, k);
	//Look for the index in the hashtable.
	index = indexFor (iht->tablelength, hashvalue);
	shmht_debug (("shmht_search: Index for this key: %d", index));
	//Calcule the offset:
	index_Entry = h->entrypoint + (index * sizeof (struct entry));
	while (index_Entry != NULL && index_Entry->used) {
		/* Check hash value to short circuit heavier comparison */
		if (hashvalue == index_Entry->h
			&&
			!compareBinaryKeys (index_Entry->key_size,
								(void *) index_Entry->k, key_size, k)) {
			shmht_debug (("shmht_search: finded shmht_search!\n"));
			//Look for the bucket. 
			//Calcule it as: buckets offset + number * sizeof(complete bucket) 
			//+ sizeof(bucket structure)
			retValue = h->bucketmarket +
				(index_Entry->bucket *
				 (sizeof (struct bucket) + iht->registry_max_size))
				+ sizeof (struct bucket);
			(*returned_size) = index_Entry->bucket_stored_size;

			//Paranoid check ;)
			struct bucket *target_bucket = h->bucketmarket +
				(index_Entry->bucket *
				 (sizeof (struct bucket) + iht->registry_max_size));
			assert (target_bucket->used == 1
					|| "Logical Error: found an entry with Empty bucket");

			break;
		}

		//If there is not in the entries... look in colisions :D
		index_Entry = (index_Entry->next != -1) ?
			h->collisionentries + (index_Entry->next * sizeof (struct entry))
			: NULL;
	}
	read_unlock (iht->semaphore);

	return retValue;
}								// shmht_search

/*****************************************************************************/


/*
  Since hashtable_remove can be called from a locked context, I've extracted the logic
  into an internal function, and left only the lock/unlock logic in the hastable_remove
  function.
  To call from a locked context, call __shmht_remove__ instead hashtable_remove
 */
static int
__shmht_remove__ (struct shmht *h, void *k, size_t key_size)
{
	struct internal_hashtable *iht = h->internal_ht;
	struct entry *index_Entry = NULL;
	struct entry *previous_Entry = NULL;	// previous Entry
	int retValue = 0;
	unsigned int hashvalue, index;

	hashvalue = hash (h, k);
	index = indexFor (iht->tablelength, hash (h, k));

	shmht_debug (("__shmht_remove__: Index for this key: %d\n", index));
	//Calcule the offset:
	index_Entry = h->entrypoint + (index * sizeof (struct entry));
	while (index_Entry != NULL && index_Entry->used) {
		/* Check hash value to short circuit heavier comparison */
		if (hashvalue == index_Entry->h
			&&
			!compareBinaryKeys (index_Entry->key_size,
								(void *) index_Entry->k, key_size, k)) {
			shmht_debug (("__shmht_remove__: finded hashtable_remove!\n"));
			break;
		}

		//If there is not in the entries... look in colisions :D
		previous_Entry = index_Entry;
		index_Entry = (index_Entry->next != -1) ?
			h->collisionentries + (index_Entry->next * sizeof (struct entry))
			: NULL;
	}

	//If the key has been found:
	if (index_Entry != NULL && index_Entry->used) {
		//First, mark the bucket as not used.
		struct bucket *target_bucket = h->bucketmarket +
			(index_Entry->bucket *
			 (sizeof (struct bucket) + iht->registry_max_size));
		target_bucket->used = 0;
		//+1 to the retValue (by default 0)
		retValue += 1;
		//Decrease the hash table entry count.
		iht->entrycount -= 1;
		if (!previous_Entry) {
			//The found instance is NOT stored in Colision.
			//So, we must copy to Entries the first of Colision.
			if (index_Entry->next != -1) {
				//Found the copy
				struct entry *next_Entry =
					h->collisionentries +
					(index_Entry->next * sizeof (struct entry));
				//Direct copy of the context of the next entry into entries.
				int aux_position = index_Entry->position;
				(*index_Entry) = (*next_Entry);
				//Delete the "used" from the entry
				next_Entry->used = 0;
				//Set the correct position
				index_Entry->position = aux_position;
			}
			else				//There is not colision.
				index_Entry->used = 0;
		}
		else {
			//The found instance is stored in Colision:
			previous_Entry->next = index_Entry->next;
			//Mark the entry as not used.
			index_Entry->used = 0;
		}
	}
	// Now we're in a consistent state.

	return retValue;
}								// __shmht_remove__


/****************************************************************************/
int
shmht_remove (struct shmht *h, void *k, size_t key_size)
{
	struct internal_hashtable *iht = h->internal_ht;
	if (write_lock (iht->semaphore) < 0)
		return -ECANCELED;
	int retValue = __shmht_remove__ (h, k, key_size);
	write_unlock (iht->semaphore);
	return retValue;
}								// hashtable_remove

/*****************************************************************************/

int
shmht_flush (struct shmht *h)
{

	struct internal_hashtable *iht = h->internal_ht;
	if (write_lock (iht->semaphore) < 0)
		return -ECANCELED;
	int i;
	//First, clear all the buckets:
	for (i = 0; i < iht->tablelength; i++) {
		struct bucket *target_bucket = h->bucketmarket +
			(i * (sizeof (struct bucket) + iht->registry_max_size));
		target_bucket->used = 0;
	}

	struct entry *target_entry;
	//Second, clear all the entries:
	for (i = 0; i < iht->tablelength; i++) {
		target_entry = h->entrypoint + (i * sizeof (struct entry));
		target_entry->used = 0;
	}

	//Last, clear all the colisions.
	for (i = 0; i < iht->tablelength; i++) {
		target_entry = h->collisionentries + (i * sizeof (struct entry));
		target_entry->used = 0;
	}
	iht->entrycount = 0;
	write_unlock (iht->semaphore);
	return 0;

}								// shmht_flush

/****************************************************************************/

int
shmht_remove_older_entries (struct shmht *h, int p)
{
	//Define a local aux structure ;D
	struct finder_aux_struct
	{
		long sec;
		int is_in_col;
		int index;
	};
	//Define a local function.
	void insert_older_if_necessary (struct entry *e,
									struct finder_aux_struct *into,
									unsigned int size_of_into, int is_in_col)
	{

		int i;
		//Var that points to the place to replace.
		struct finder_aux_struct *aux = NULL;
		for (i = 0; i < size_of_into; i++){
		  //x		  printf ("Comparison => entry: %ld, struct: %ld \n", e->sec, into[i].sec);
			if (e->sec < into[i].sec) {
				//If it's -1, it's not initiated, stop.
				aux = &into[i];
				break;
			}
		}

		if (aux != NULL) {
			//In aux, we have the element to replace.
			shmht_debug (("insert_older_if_necessary: Inserted Entry : secs %ld, is_in_col: %d, position: %d \n"
						  , e->sec, is_in_col, e->position));
			aux->sec = e->sec;
			aux->is_in_col = is_in_col;
			aux->index = e->position;
		}
	}							// insert_older_if_necessary



	//Check before lock:
	if (p > 100)
		return -EINVAL;

	struct internal_hashtable *iht = h->internal_ht;
	if (write_lock (iht->semaphore) < 0)
		return -ECANCELED;

	//Start the stuff
	int retValue = 0;
	//Calcule the number of entries to delete:
	int deleteEntries = iht->tablelength * p / 100;

	shmht_debug (("shmht_remove_older_entries: Number of entries to Delete: %d\n", deleteEntries));

	struct finder_aux_struct older_storage[deleteEntries];

	int i;
	//Init the struct:
	for (i = 0; i < deleteEntries; i++) {
		older_storage[i].sec = LONG_MAX;
		older_storage[i].is_in_col = -1;
		older_storage[i].index = -1;
	}

	//Go around the structs:
	//Mantain ordered the structure.
	struct entry *target_entry;
	//Second, clear all the entries:
	for (i = 0; i < iht->tablelength; i++) {
		target_entry = h->entrypoint + (i * sizeof (struct entry));
		if (target_entry->used)
			insert_older_if_necessary (target_entry, older_storage,
									   deleteEntries, 0);
	}

	//Last, clear all the colisions.
	for (i = 0; i < iht->tablelength; i++) {
		target_entry = h->collisionentries + (i * sizeof (struct entry));
		if (target_entry->used)
			insert_older_if_necessary (target_entry, older_storage,
									   deleteEntries, 1);
	}


	//Now, delete the entries stored in older_storage:
	for (i = 0; i < deleteEntries; i++) {
		if (older_storage[i].index != LONG_MAX) {
			if (!older_storage[i].is_in_col)
				target_entry =
					h->entrypoint +
					(older_storage[i].index * sizeof (struct entry));
			else
				target_entry =
					h->collisionentries +
					(older_storage[i].index * sizeof (struct entry));
			__shmht_remove__ (h, target_entry->k, target_entry->key_size);
			retValue++;
		}
		else
			break;
	}							//for

	write_unlock (iht->semaphore);
	return retValue;
}								// shmht_remove_older_entries


/****************************************************************************/

/* destroy, it destroys all the shared memory and the semaphores :P*/
int
shmht_destroy (struct shmht *h)
{
	struct internal_hashtable *iht = h->internal_ht;
	//Wait untill there are not more processess.
	WRITE_LOCK_READERS (iht->semaphore);
	WRITE_LOCK_TO_WRITE (iht->semaphore);
	//Now it's locket, but now, destroy the semaphore.
	//The other clients will recieve an EIDRM in shmget
	semctl (iht->semaphore, 0, IPC_RMID);
	//Delete the shared memory.
	shmctl (iht->shmid, IPC_RMID, NULL);
	return 0;
}								// shmht_destroy
