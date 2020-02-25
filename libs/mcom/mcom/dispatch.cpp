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

#include "dispatch.hpp"

namespace dispatch {

Queue::Queue() : queue_(dispatch_get_global_queue(0, 0)) {}

Queue::Queue(const Queue &other) : queue_{other.queue_} {
  if (queue_) {
    dispatch_retain(queue_);
  }
}

Queue::Queue(Queue &&other) : queue_{other.queue_} { other.queue_ = nullptr; }

Queue::Queue(const char *name)
    : queue_(dispatch_queue_create(name, DISPATCH_QUEUE_SERIAL)) {}

Queue::~Queue() {
  if (queue_) {
    dispatch_release(queue_);
  }
}

Queue Queue::Main() { return Queue{dispatch_get_main_queue()}; }

void Queue::Async(dispatch_block_t block) const {
  dispatch_async(queue_, block);
}

Queue::Queue(dispatch_queue_t queue) : queue_{queue} {}

const Time Time::kForever{DISPATCH_TIME_FOREVER};

Group::Group() : group_{dispatch_group_create()} {}

Group::Group(const Group &other) : group_{other.group_} {
  if (group_) {
    dispatch_retain(group_);
  }
}

Group::Group(Group &&other) : group_{other.group_} { other.group_ = nullptr; }

Group::~Group() {
  if (group_) {
    dispatch_release(group_);
  }
}

bool Group::Wait(const Time &time) {
  return 0 == dispatch_group_wait(group_, time.Value());
}

Semaphore::Semaphore(long count) : sema_(dispatch_semaphore_create(count)) {}

Semaphore::~Semaphore() { dispatch_release(sema_); }

bool Semaphore::Wait(const Time &time) {
  return (0 == dispatch_semaphore_wait(sema_, time.Value()));
}

bool Semaphore::Signal() { return (0 != dispatch_semaphore_signal(sema_)); }

Source::Source(Source &&other) : source_{other.source_} {
  other.source_ = nullptr;
}

Source &Source::operator=(Source &&other) {
  if (source_) {
    dispatch_release(source_);
  }
  source_ = other.source_;
  other.source_ = nullptr;
  return *this;
}

Source::~Source() {
  if (source_) {
    dispatch_release(source_);
  }
}

void Source::Resume() { dispatch_resume(source_); }

void Source::Suspend() { dispatch_suspend(source_); }

void Source::Cancel() { dispatch_source_cancel(source_); }

Timer::Timer(const std::optional<Queue> &queue)
    : Source{dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                    queue ? queue->operator*() : nullptr)} {}

void Timer::Schedule(dispatch::Time time, std::optional<Duration> duration) {
  dispatch_source_set_timer(
      source_, time.Value(),
      duration ? duration->Count() : DISPATCH_TIME_FOREVER, 0);
}

MachReceiveSource::MachReceiveSource(mach_port_name_t name,
                                     const std::optional<Queue> &queue)
    : Source{dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, name, 0,
                                    queue ? queue->operator*() : nullptr)} {}

ProcessExitSource::ProcessExitSource(pid_t pid,
                                     const std::optional<Queue> &queue)
    : Source{dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid,
                                    DISPATCH_PROC_EXIT,
                                    queue ? queue->operator*() : nullptr)} {}

}  // namespace dispatch
