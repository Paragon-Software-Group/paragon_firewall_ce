// Paragon Firewall Community Edition
// Copyright (C) 2019-2020  Paragon Software GmbH
//
// This file is part of Paragon Firewall Community Edition.
//
// Paragon Firewall Community Edition is free software: you can
// redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
// Paragon Firewall Community Edition is distributed in the hope that it
// will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
// the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Paragon Firewall Community Edition. If not, see
//   <https://www.gnu.org/licenses/>.

#pragma once

#include <functional>
#include <optional>
#include <type_traits>

#include <dispatch/dispatch.h>

namespace dispatch {

#define MCOM_ONCE(...)                         \
  [&]() -> decltype(auto) {                    \
    static dispatch_once_t token;              \
    return dispatch::Once(token, __VA_ARGS__); \
  }()

class Time;

class Queue {
 public:
  explicit Queue();
  explicit Queue(const char *name);
  Queue(const Queue &);
  Queue(Queue &&);
  ~Queue();

  static Queue Main();

  void Async(dispatch_block_t block) const;

  template <class Fn>
  void Async(Fn &&fn) const;

  template <class Fn>
  auto Sync(Fn &&fn) const -> decltype(fn());

  template <class Fn>
  void After(Time when, Fn &&fn);

  dispatch_queue_t operator*() const { return queue_; }

 private:
  explicit Queue(dispatch_queue_t queue);

  dispatch_queue_t queue_;
};

class Duration {
 public:
  inline static Duration Seconds(int64_t count) {
    return Duration{count * int64_t(NSEC_PER_SEC)};
  }

  int64_t Count() const { return count_; }

 private:
  constexpr Duration(int64_t count) : count_(count) {}

  int64_t count_;
};

class Time {
 public:
  static const Time kForever;

  static Time Now() { return dispatch_time(DISPATCH_TIME_NOW, 0); }
  static Time WallNow();

  Time operator+(const Duration &offset) const {
    return dispatch_time(time_, offset.Count());
  }

  dispatch_time_t Value() const { return time_; }

 private:
  constexpr Time(dispatch_time_t t) : time_(t) {}

  dispatch_time_t time_;
};

class Group {
 public:
  explicit Group();

  Group(const Group &);
  Group(Group &&);

  Group &operator=(const Group &);
  Group &operator=(Group &&);

  ~Group();

  template <class Fn>
  void Async(const Queue &queue, Fn &&fn) const;

  bool Wait(const Time &time);

 private:
  dispatch_group_t group_;
};

class Semaphore {
 public:
  Semaphore(long count);
  Semaphore(const Semaphore &);
  Semaphore(Semaphore &&);
  ~Semaphore();

  bool Wait(const Time &);

  bool Signal();

  class Guard {
   public:
    Guard(Semaphore &sema) : sema_{sema} { sema_.Wait(Time::kForever); }

    ~Guard() { sema_.Signal(); }

   private:
    Semaphore &sema_;
  };

  Guard Lock() { return {*this}; }

 private:
  dispatch_semaphore_t sema_;
};

class Source {
 public:
  Source(Source &&);

  Source &operator=(Source &&);

  ~Source();

  template <class Fn>
  void SetEventHandler(Fn &&fn);

  template <class Fn>
  void SetCancelHandler(Fn &&fn);

  void Resume();

  void Suspend();

  void Cancel();

 private:
  friend class Timer;
  friend class MachReceiveSource;
  friend class ProcessExitSource;

  Source(dispatch_source_t source) : source_{source} {}

  dispatch_source_t source_;
};

class Timer : public Source {
 public:
  explicit Timer(const std::optional<Queue> &queue = std::nullopt);

  void Schedule(Time, std::optional<Duration> duration = std::nullopt);
};

class MachReceiveSource : public Source {
 public:
  MachReceiveSource(mach_port_name_t name,
                    const std::optional<Queue> &queue = std::nullopt);
};

class ProcessExitSource : public Source {
 public:
  ProcessExitSource(pid_t pid,
                    const std::optional<Queue> &queue = std::nullopt);
};

template <class Fn>
auto Once(dispatch_once_t &token, Fn &&fn) -> decltype(fn()) & {
  using value_type = decltype(fn());

  union Store {
    Store() {}
    ~Store() {}

    value_type value;
  };

  struct Context {
    static void Apply(void *context_ptr) {
      auto &context = *static_cast<Context *>(context_ptr);

      new (std::addressof(context.store.value)) value_type(context.fn());
    }

    Fn &fn;
    Store &store;
  };

  static Store store;
  Context context{fn, store};

  dispatch_once_f(&token, &context, Context::Apply);

  return store.value;
}

template <class Fn>
auto Once(dispatch_once_t &token, Fn fn)
    -> std::enable_if_t<std::is_same<decltype(fn()), void>::value> {
  struct Context {
    static void Apply(void *context_ptr) {
      (*static_cast<Fn *>(context_ptr))();
    }
  };

  dispatch_once_f(&token, &fn, Context::Apply);
}

template <class Fn>
void Queue::Async(Fn &&fn) const {
  auto fn_ptr = std::make_shared<Fn>(std::forward<Fn>(fn));

  dispatch_async(queue_, ^{
    (*fn_ptr)();
  });
}

template <class Fn>
void Queue::After(Time when, Fn &&fn) {
  auto fn_ptr = std::make_shared<Fn>(std::forward<Fn>(fn));

  dispatch_after(when.Value(), queue_, ^{
    (*fn_ptr)();
  });
}

template <class Fn>
auto Queue::Sync(Fn &&fn) const -> decltype(fn()) {
  using Result = decltype(fn());
  Fn *fn_ptr = &fn;

  if constexpr (std::is_void_v<Result>) {
    dispatch_sync(queue_, ^{
      (*fn_ptr)();
    });
  } else {
    std::optional<Result> result;
    std::optional<Result> *result_ptr = &result;

    dispatch_sync(queue_, ^{
      result_ptr->emplace((*fn_ptr)());
    });

    return *std::move(result);
  }
}

template <class Fn>
void Group::Async(const Queue &queue, Fn &&fn) const {
  auto fn_ptr = std::make_shared<Fn>(std::forward<Fn>(fn));

  dispatch_group_async(group_, *queue, ^{
    (*fn_ptr)();
  });
}

template <class Fn>
void Source::SetEventHandler(Fn &&fn) {
  auto fn_ptr = std::make_shared<Fn>(std::forward<Fn>(fn));

  dispatch_source_set_event_handler(source_, ^{
    (*fn_ptr)();
  });
}

template <class Fn>
void Source::SetCancelHandler(Fn &&fn) {
  auto fn_ptr = std::make_shared<Fn>(std::forward<Fn>(fn));

  dispatch_source_set_cancel_handler(source_, ^{
    (*fn_ptr)();
  });
}

}  // namespace dispatch
