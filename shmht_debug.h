#ifndef __HASHTABLE_DEBUG__HH__
#define __HASHTABLE_DEBUG__HH__

//#define __SHMT_DEBUG_MODE__

#ifdef __SHMT_DEBUG_MODE__
#include <stdio.h>
#define shmht_debug(a) printf a
#else
#define shmht_debug(a)
#endif

#endif //__HASHTABLE_DEBUG__HH__
