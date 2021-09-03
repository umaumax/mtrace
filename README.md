# mtrace

pthread_mutex tracer

## trace functions progress
* [x] pthread_mutex_lock
* [x] pthread_mutex_unlock
* [ ] pthread_mutex_trylock
* [ ] pthread_mutex_timedlock
* [x] pthread_cond_signal
* [x] pthread_cond_broadcast
* [x] pthread_cond_wait
* [x] pthread_cond_timedwait
* [ ] pthread_rwlock_rdlock
* [ ] pthread_rwlock_tryrdlock
* [ ] pthread_rwlock_wrlock
* [ ] pthread_rwlock_trywrlock
* [ ] pthread_rwlock_unlock
* [ ] std::mutex(lock, try_lock, unlock)
* [ ] std::recursive_mutex(lock, try_lock, unlock)
* [ ] std::timed_mutex(lock, try_lock, try_lock_for, try_lock_until, unlock)

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

## problems
### 時系列データとして扱うと、データ量が膨大となり、トレースや可視化の限界がある
内部でまとめられる処理をまとめて、出力する情報を整理する

例えば、特定の値をカウントしておいて、統計情報を出力したり、graphvizのようなグラフにする

カウントする値の候補
* lock関数の処理時間
* cond_wait関数の処理時間
* tid
* mutexのid

#### トレース情報のフィルターを考える
* 特定のTIDのみトレース
  * スレッド間でmutexは利用するので、これだと予め、どのスレッドで利用するが既知でないと厳しい
* 特定のアドレスのmutexのみトレース
  * これがよさそうだが、実行時にtraceしたいmutexを決めるには、予めソースコードにそのためのコードを仕込む必要がありそう
* cond_waitとsignalのみとするバージョンを試してみる?
