#ifndef LOCKQUEUE_H
#define LOCKQUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>

// 异步写日志使用的线程安全队列
template <class T> class LockQueue {
public:
  void Push(const T &msg) {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _queue.push(msg);
    }
    _condvar.notify_one();
  }

  T Pop() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_queue.empty()) {
      _condvar.wait(lock);
    }

    T msg = _queue.front();
    _queue.pop();
    return msg;
  }

private:
  std::queue<T> _queue;
  std::mutex _mutex;
  std::condition_variable _condvar;
};

#endif
