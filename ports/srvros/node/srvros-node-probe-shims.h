#ifndef SRVROS_NODE_PROBE_SHIMS_H
#define SRVROS_NODE_PROBE_SHIMS_H

#ifndef __ros__
#define __ros__ 1
#endif

#if defined(_LIBCPP_HAS_NO_THREADS)
namespace std {
class thread {
public:
  static unsigned hardware_concurrency() noexcept;
};
class mutex {
public:
  void lock() noexcept {}
  void unlock() noexcept {}
};
enum class cv_status { no_timeout, timeout };
template <typename Mutex>
class unique_lock {
public:
  explicit unique_lock(Mutex& mutex) : mutex_(&mutex) { mutex_->lock(); }
  ~unique_lock() { if (mutex_ != nullptr) mutex_->unlock(); }
private:
  Mutex* mutex_;
};
template <typename Mutex>
class lock_guard {
public:
  explicit lock_guard(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
  ~lock_guard() { mutex_.unlock(); }
private:
  Mutex& mutex_;
};
class condition_variable {
public:
  void notify_one() noexcept {}
  template <typename Lock>
  void wait(Lock&) {}
  template <typename Lock, typename Duration>
  cv_status wait_for(Lock&, const Duration&) {
    return cv_status::timeout;
  }
  template <typename Lock, typename TimePoint>
  cv_status wait_until(Lock&, const TimePoint&) {
    return cv_status::timeout;
  }
};
namespace this_thread {
inline void yield() noexcept {}
}  // namespace this_thread
}  // namespace std
#endif

#endif
