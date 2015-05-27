#pragma once

#include <mutex>
#include <condition_variable>

namespace MT {
/** @addtogroup mt
 *  @{
 */

/** @brief Synchronous flag
 *
 * Flag used to synchronously exchange data between threads.
 *
 * @tparam T  type of data
 */
template <typename T>
class SFlag {
public:
  /** @brief Constructor
   *
   * @param val  initial value
   */
  SFlag (const T & val)
    : val_ (val)
  {}

  /** @brief Set the flag value
   *
   * This is a non-blocking operation. If other threads are waiting for an
   * updated flag value, they are notified.
   *
   * @param val  new flag value
   */
  void set (const T & val) {
    {
      std::lock_guard<std::mutex> lock (mtx_);
      val_  = val;
      flag_ = true;
    }
    cond_.notify_one();
  }

  /** @brief Wait for the flag to be updated and retrieve the new value.
   *
   * This operation blocks until another thread updates the new value (using
   * @ref set). The updated value is then returned.
   *
   * @return updated flag value
   */
  T get () {
    std::unique_lock<std::mutex> lock (mtx_);
    flag_ = false;
    while (!flag_) {
      cond_.wait(lock);
    }
    return val_;
  }

private:
  std::condition_variable cond_;
  std::mutex mtx_;
  bool flag_;
  T val_;
};

/** @} */
}
