# mtrace

mutex tracer

* [*] pthread_mutex_lock
* [*] pthread_mutex_unlock
* [*] pthread_cond_signal
* [*] pthread_cond_broadcast
* [*] pthread_cond_wait
* [*] pthread_cond_timedwait
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
./ltrace-parse.py <(cat mtrace.out.* | sort -k 2) | jq > mtrace.json
```
