#ifndef __HASHTABLE_CWC22_H__
#define __HASHTABLE_CWC22_H__

#include <unistd.h>

struct shmht;

/*! \mainpage lib_shmht
 *
 * \section Introduction
 *
 * The lib_shmht is a full-functionall hashtable implementation in shared memory.<BR>
 * This library is designed for high-performace caches, but it can be used for
 * many pourpouses. <BR>
 * Design principles of the lib_shmht: <BR>
 * <b>no-reallocation</b> of the shared memory. So, the HT can't grow in size from it's creation, but you
 * can define the % of older entries that can be erased when a element has not enought
 * space.<BR>
 * <b>concurrency</b>: The accesses are controlled by a semaphore R/W lock implemntation, so not concurrency
 * problem is possible.<BR>
 * <b>performance</b>: The performance is the main target of this implementation.<BR>
 *
 * It has the next <b>limitations</b>: <BR>
 *  * Key size limited by compiling time.<BR>
 *  * The erased elements are the oldests, not the less used. This is to don't write (and lock the entire ht)
 * in all the reads.<BR>
 *
 * Please, check the design file if you want more specifical data.
 * 
 */


/*!   
 * @name                    shmht_hashtable
 * @param   name            Name of the HashTable.
 * @param   number          Number of records.
 * @param   size            Size of each record.
 * @param   hashfunction    function for hashing keys.
 * @param   key_eq_fn       function for determining key equality
 * @return                  newly created hashtable or NULL on failure
 *
 * This function, takes the existing shared memory, or creates if it does not
 * exists.
 * The name is the primary key of the shared memory hash table. If it exists, 
 * the create_hashtable will asociate with the apropiate shared memory.
 *
 */

struct shmht *create_shmht(char *name,
				unsigned int number,
				size_t size,
				unsigned int (*hashfunction) (void *),
				int (*key_eq_fn) (void *, void *));

/*!   
 * @name        shmht_insert
 * @param   h   the hashtable to insert into
 * @param   k   the key - hashtable claims ownership and will free on removal
 * @param   v   the value - does not claim ownership
 * @return      > zero for successful insertion, else for error.
 *
 * This function does not check for repeated insertions with a duplicate key.
 * The value returned when using a duplicate key is undefined.
 * If in doubt, remove before insert.
 * The size of this hashtable is fixed, so if the hashtable is full, the insert
 * will fail.
 */

int
shmht_insert (struct shmht *h, void *k, size_t key_size, void *v,
				  size_t value_size);


/*!   
 * @name        shmht_search
 * @param   h   the hashtable to search
 * @param   k   the key to search for  - does not claim ownership
 * @param key_size Size of the key.
 * @param returned_size [out], the size of the returned value.
 * @return      the value associated with the key, or NULL if none found
 * 
 * You should be careful, beacause, this function returns a pointer to the 
 * shared memory area. DO NOT FREE THIS POINTER!
 */

void *shmht_search (struct shmht *h, void *k, size_t key_size,
						size_t * returned_size);


/*!   
 * @name        shmht_remove
 * @param   h   the hashtable to remove the item from
 * @param   k   the key to search for  - does not claim ownership
 * @param returned_size  [out] returns the size of the returned void *.
 * @return      the number of items removed.
 */

int shmht_remove (struct shmht *h, void *k, size_t key_size);



/*!   
 * @name        shmht_count
 * @param   h   the hashtable
 * @return      the number of items stored in the hashtable, -1 if something
 *              nasty happens.
 */
int shmht_count (struct shmht *h);


/*!   
 * @name        shmht_flush
 * @param   h   the hashtable
 * @return      0 if not problem, <0 if error.
 */

int shmht_flush (struct shmht *h);

/*!
 * @name        shmht_remove_older_entries
 * @param   h   the hashtable
 * @param   p   the % of older values to erase
 * @return      The number of deleted entries.
 */

int shmht_remove_older_entries (struct shmht *h, int p);

/*!   
 * @name        shmht_destroy
 * @param   h   the hashtable
 * 
 * Be careful with this operation, beacause all the processes that are currently
 * using the shared memory hash table will fail (it deletes the shared memory and
 * the semaphores used as mutex). 
 * If you have any doubt, use the hashtable_flush, instead of this function.
 */

int shmht_destroy (struct shmht *h);

#endif /* __HASHTABLE_CWC22_H__ */
