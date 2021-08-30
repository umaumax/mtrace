#include <dlfcn.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace {
void exit_handler(int sig) { std::exit(128 + sig); }
struct ExitBySignal {
  ExitBySignal() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = exit_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
  }
} exit_by_signal;
}  // namespace

#if __APPLE__
pid_t gettid() {
  uint64_t tid64 = 0;
  pthread_threadid_np(nullptr, &tid64);
  return static_cast<pid_t>(tid64);
}
#elif __linux__
pid_t gettid() { return syscall(SYS_gettid); }
#else
#error "Non supported os"
#endif

uint64_t exp2_roundup(uint64_t x) {
  if (x == 0) {
    return 0;
  }

  int v      = 1;
  uint64_t y = x;
  while (y >>= 1) {
    v <<= 1;
  }
  if (x > v) {
    v <<= 1;
  }
  return v;
}

class Counter {
 public:
  Counter() {}

  void Add(uint64_t value) {
    std::string key =
        std::to_string((double)(exp2_roundup(value / 1000 / 1000)) / 1000.0 /
                       1000.0 * 1000.0 * 1000.0);
    const int max_key_length = 16;
    if (key.length() < max_key_length) {
      key = std::string(max_key_length - key.length(), ' ') + key;
    }
    data_[key]++;
  }

  std::map<std::string, int> data_;
};

thread_local pid_t current_tid = gettid();
class Logger {
 public:
  Logger() {
    ofs_.open("./mtrace.out." + std::to_string(current_tid));
    if (!ofs_) {
      std::cerr << "Failed to open file" << std::endl;
      std::exit(1);
    }
  }
  ~Logger() {
    if (ofs_) {
      ofs_.close();
    }
  }

  void Write(const std::string &text) {
    ofs_ << current_tid << " " << text << std::endl;
  }

 private:
  std::ofstream ofs_;
};
thread_local Logger logger;

class CounterManager {
 public:
  CounterManager(std::string name) : name_(name) {}
  ~CounterManager() {
    std::cout << "name:" << name_ << std::endl;
    std::cout << "tid:" << current_tid << std::endl;
    std::cout << "# elapsed timep[ms]" << std::endl;
    for (auto &v : counter_map_) {
      const std::string &address = v.first;
      Counter &counter           = v.second;
      for (auto &v : counter.data_) {
        const std::string &value = v.first;
        int count                = v.second;
        std::cout << address << " : " << value << ", " << count << std::endl;
      }
    }
  }

  Counter &Get(std::string key) {
    if (counter_map_.find(key) == counter_map_.end()) {
      counter_map_.insert(std::make_pair(key, Counter()));
    }
    return counter_map_[key];
  }

  std::string name_;
  std::map<std::string, Counter> counter_map_;
};
thread_local CounterManager mutex_counter_manager("mutex");
thread_local CounterManager cond_counter_manager("cond");

#define HOOK(ret_type, func_name, ...)                       \
  ret_type func_name(__VA_ARGS__);                           \
  namespace {                                                \
  using func_name##_type            = decltype(&func_name);  \
  func_name##_type orig_##func_name = []() {                 \
    auto f = (func_name##_type)dlsym(RTLD_NEXT, #func_name); \
    assert(f && "failed dlsym " #func_name);                 \
    return f;                                                \
  }();                                                       \
  }                                                          \
  ret_type func_name(__VA_ARGS__)

extern "C" {
HOOK(int, pthread_mutex_lock, pthread_mutex_t *mutex) {
  std::chrono::system_clock::time_point hooked_func_start =
      std::chrono::system_clock::now();
  int ret = orig_pthread_mutex_lock(mutex);
  double hooked_func_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now() - hooked_func_start)
          .count() /
      1000.0 / 1000.0 / 1000.0;
  double timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         hooked_func_start.time_since_epoch())
                         .count() /
                     1000.0 / 1000.0;
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6) << timestamp << " "
     << "pthread_mutex_lock(mutex=" << mutex << ") = " << ret << " <"
     << hooked_func_elapsed << ">";
  logger.Write(ss.str());
  std::string key = std::to_string((uintptr_t)mutex);
  uint64_t elapsed_time =
      (uint64_t)(hooked_func_elapsed * 1000.0 * 1000.0 * 1000.0);
  mutex_counter_manager.Get(key).Add(elapsed_time);
  return ret;
}

HOOK(int, pthread_mutex_unlock, pthread_mutex_t *mutex) {
  std::chrono::system_clock::time_point hooked_func_start =
      std::chrono::system_clock::now();
  int ret = orig_pthread_mutex_unlock(mutex);
  double hooked_func_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now() - hooked_func_start)
          .count() /
      1000.0 / 1000.0 / 1000.0;
  double timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         hooked_func_start.time_since_epoch())
                         .count() /
                     1000.0 / 1000.0;
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6) << timestamp << " "
     << "pthread_mutex_unlock(mutex=" << mutex << ") = " << ret << " <"
     << hooked_func_elapsed << ">";
  logger.Write(ss.str());
  return ret;
}

HOOK(int, pthread_cond_signal, pthread_cond_t *cond) {
  std::chrono::system_clock::time_point hooked_func_start =
      std::chrono::system_clock::now();
  int ret = orig_pthread_cond_signal(cond);
  double hooked_func_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now() - hooked_func_start)
          .count() /
      1000.0 / 1000.0 / 1000.0;
  double timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         hooked_func_start.time_since_epoch())
                         .count() /
                     1000.0 / 1000.0;
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6) << timestamp << " "
     << "pthread_cond_signal(cond=" << cond << ") = " << ret << " <"
     << hooked_func_elapsed << ">";
  logger.Write(ss.str());
  return ret;
}

HOOK(int, pthread_cond_broadcast, pthread_cond_t *cond) {
  std::chrono::system_clock::time_point hooked_func_start =
      std::chrono::system_clock::now();
  int ret = orig_pthread_cond_broadcast(cond);
  double hooked_func_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now() - hooked_func_start)
          .count() /
      1000.0 / 1000.0 / 1000.0;
  double timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         hooked_func_start.time_since_epoch())
                         .count() /
                     1000.0 / 1000.0;
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6) << timestamp << " "
     << "pthread_cond_broadcast(cond=" << cond << ") = " << ret << " <"
     << hooked_func_elapsed << ">";
  logger.Write(ss.str());
  return ret;
}

HOOK(int, pthread_cond_wait, pthread_cond_t *cond, pthread_mutex_t *mutex) {
  std::chrono::system_clock::time_point hooked_func_start =
      std::chrono::system_clock::now();
  int ret = orig_pthread_cond_wait(cond, mutex);
  double hooked_func_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now() - hooked_func_start)
          .count() /
      1000.0 / 1000.0 / 1000.0;
  double timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         hooked_func_start.time_since_epoch())
                         .count() /
                     1000.0 / 1000.0;
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6) << timestamp << " "
     << "pthread_cond_wait(cond=" << cond << ", mutex=" << mutex
     << ") = " << ret << " <" << hooked_func_elapsed << ">";
  logger.Write(ss.str());

  std::string key = std::to_string((uintptr_t)mutex);
  uint64_t elapsed_time =
      (uint64_t)(hooked_func_elapsed * 1000.0 * 1000.0 * 1000.0);
  cond_counter_manager.Get(key).Add(elapsed_time);
  return ret;
}

HOOK(int, pthread_cond_timedwait, pthread_cond_t *cond, pthread_mutex_t *mutex,
     const struct timespec *abstime) {
  std::chrono::system_clock::time_point hooked_func_start =
      std::chrono::system_clock::now();
  int ret = orig_pthread_cond_timedwait(cond, mutex, abstime);
  double hooked_func_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now() - hooked_func_start)
          .count() /
      1000.0 / 1000.0 / 1000.0;
  double timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         hooked_func_start.time_since_epoch())
                         .count() /
                     1000.0 / 1000.0;
  if (abstime != nullptr) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6) << timestamp << " "
       << "pthread_cond_timedwait(cond=" << cond << ", mutex=" << mutex
       << ", abstime.tv_sec=" << (int)(abstime->tv_sec)
       << ", abstime.tv_nsec=" << abstime->tv_nsec << ") = " << ret << " <"
       << hooked_func_elapsed << ">";
    logger.Write(ss.str());
  } else {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6) << timestamp << " "
       << "pthread_cond_timedwait(cond=" << cond << ", mutex=" << mutex
       << ", abstime=" << abstime << ") = " << ret << " <"
       << hooked_func_elapsed << ">";
    logger.Write(ss.str());
  }

  std::string key = std::to_string((uintptr_t)mutex);
  uint64_t elapsed_time =
      (uint64_t)(hooked_func_elapsed * 1000.0 * 1000.0 * 1000.0);
  cond_counter_manager.Get(key).Add(elapsed_time);
  return ret;
}
}
