# libshmht

------

**Shared memory hash table for cache pourposes.**

Forked from Jonbaine's project @ http://sourceforge.net/projects/libshmht/ (r11), keeping only core files and an ugly Makefile which just works.

Features
======

* Open source - AGPL licensed.
* Clear and simple API
* Developed with the performance as main target
* Not resizes during insertions (fixed size from creation)

Stability
======

You may be glad to know Jonbaine's reply to my concern on it's stability on [StackOverflow][2]:

>Yes, it's deployed into a high load environment without any issue as openssl interprocess cache.

------

p.s. To compile `shmht_tests`, you should have [cgreen][1] installed first.

[1]: http://sourceforge.net/projects/cgreen/
[2]: http://stackoverflow.com/questions/5384218/c-putting-a-hash-table-into-a-shared-memory-segment/14626511?noredirect=1#comment35441036_14626511
