#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

const size_t loop_max = 10;

pthread_mutex_t m   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int counter         = 0;

void f1();
void f2();

int main(int argc, char* argv[]) {
  pthread_t thread1, thread2;
  int ret1, ret2;

  ret1 = pthread_create(&thread1, nullptr, (void* (*)(void*))f1, nullptr);
  ret2 = pthread_create(&thread2, nullptr, (void* (*)(void*))f2, nullptr);

  if (ret1 != 0) {
    err(EXIT_FAILURE, "can not create thread 1: %s", strerror(ret1));
  }
  if (ret2 != 0) {
    err(EXIT_FAILURE, "can not create thread 2: %s", strerror(ret2));
  }

  pthread_mutex_lock(&m);
  while (counter < loop_max / 2) {
    pthread_cond_wait(&cond, &m);
  }
  pthread_mutex_unlock(&m);

  printf("execute pthread_join thread1\n");
  ret1 = pthread_join(thread1, nullptr);
  if (ret1 != 0) {
    err(EXIT_FAILURE, "can not join thread 1: %d", ret1);
  }

  printf("execute pthread_join thread2\n");
  ret2 = pthread_join(thread2, nullptr);
  if (ret2 != 0) {
    err(EXIT_FAILURE, "can not join thread 2: %d", ret2);
  }

  printf("done\n");
  printf("%d\n", counter);

  pthread_mutex_destroy(&m);
  return 0;
}

void f1() {
  size_t i;

  for (i = 0; i < loop_max; i++) {
    int r;
    r = pthread_mutex_lock(&m);
    if (r != 0) {
      std::cerr << "can not lock" << std::endl;
      err(EXIT_FAILURE, "can not lock: %d", r);
      continue;
    }
    counter++;
    pthread_cond_signal(&cond);
    usleep(10000);
    r = pthread_mutex_unlock(&m);
    if (r != 0) {
      std::cerr << "can not unlock" << std::endl;
      err(EXIT_FAILURE, "can not unlock: %d", r);
    }
    usleep(15000);
  }
}

void f2() {
  size_t i;

  for (i = 0; i < loop_max; i++) {
    if (pthread_mutex_lock(&m) != 0) {
      std::cerr << "can not lock" << std::endl;
      err(EXIT_FAILURE, "can not lock");
      continue;
    }
    counter++;
    // pthread_cond_signal(&cond);
    pthread_cond_broadcast(&cond);
    usleep(9000);
    if (pthread_mutex_unlock(&m) != 0) {
      std::cerr << "can not unlock" << std::endl;
      err(EXIT_FAILURE, "can not unlock");
    }
    usleep(15000);
  }
}
