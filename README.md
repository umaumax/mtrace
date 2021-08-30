# mtrace

mutex tracer

* [x] pthread_mutex_lock
* [x] pthread_mutex_unlock
* [x] pthread_cond_signal
* [x] pthread_cond_broadcast
* [x] pthread_cond_wait
* [x] pthread_cond_timedwait
* [ ] std::mutex

## how to trace
``` bash
# linux
LD_PRELOAD=./libmtrace.so ./a.out
# mac
DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=./libmtrace.so ./a.out
# or
ltrace -T -o ltrace.out -ttt -f -e 'pthread_mutex_lock' -e 'pthread_mutex_unlock' -e 'pthread_cond_signal' -e 'pthread_cond_broadcast' -e 'pthread_cond_wait' -e 'pthread_cond_timedwait' ./a.out
```

## how to parse
``` bash
./ltrace-parse.py <(cat mtrace.out.* | sort -k 2) | jq . > mtrace.json
```
