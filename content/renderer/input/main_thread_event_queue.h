// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
#define CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_

#include <deque>
#include "content/common/content_export.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/common/input/web_input_event_queue.h"
#include "content/common/input/web_input_event_traits.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

class EventWithDispatchType : public ScopedWebInputEventWithLatencyInfo {
 public:
  EventWithDispatchType(const blink::WebInputEvent& event,
                        const ui::LatencyInfo& latency,
                        InputEventDispatchType dispatch_type);
  ~EventWithDispatchType();
  bool CanCoalesceWith(const EventWithDispatchType& other) const
      WARN_UNUSED_RESULT;
  void CoalesceWith(const EventWithDispatchType& other);

  const std::deque<uint32_t>& eventsToAck() const { return eventsToAck_; }
  InputEventDispatchType dispatchType() const { return dispatch_type_; }

 private:
  InputEventDispatchType dispatch_type_;

  // |eventsToAck_| contains the unique touch event id to be acked. If
  // the events are TouchEvents the value will be 0. More importantly for
  // those cases the deque ends up containing how many additional ACKs
  // need to be sent.
  std::deque<uint32_t> eventsToAck_;
};

class CONTENT_EXPORT MainThreadEventQueueClient {
 public:
  // Handle an |event| that was previously queued (possibly
  // coalesced with another event) to the |routing_id|'s
  // channel. Implementors must implement this callback.
  virtual void HandleEventOnMainThread(
      int routing_id,
      const blink::WebInputEvent* event,
      const ui::LatencyInfo& latency,
      InputEventDispatchType dispatch_type) = 0;

  virtual void SendInputEventAck(int routing_id,
                                 blink::WebInputEvent::Type type,
                                 InputEventAckState ack_result,
                                 uint32_t touch_event_id) = 0;
};

// MainThreadEventQueue implements a queue for events that need to be
// queued between the compositor and main threads. This queue is managed
// by a lock where events are enqueued by the compositor thread
// and dequeued by the main thread.
//
// Below some example flows are how the code behaves.
// Legend: B=Browser, C=Compositor, M=Main Thread, NB=Non-blocking
//         BL=Blocking, PT=Post Task, ACK=Acknowledgement
//
// Normal blocking event sent to main thread.
//   B        C        M
//   ---(BL)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//   <-------(ACK)------
//
// Non-blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//
// Non-blocking followed by blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//         (queue)
//            ---(PT)-->
//   ---(BL)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//                  (deque)
//   <-------(ACK)------
//
class CONTENT_EXPORT MainThreadEventQueue
    : public base::RefCountedThreadSafe<MainThreadEventQueue> {
 public:
  MainThreadEventQueue(
      int routing_id,
      MainThreadEventQueueClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);

  // Called once the compositor has handled |event| and indicated that it is
  // a non-blocking event to be queued to the main thread.
  bool HandleEvent(const blink::WebInputEvent* event,
                   const ui::LatencyInfo& latency,
                   InputEventDispatchType dispatch_type,
                   InputEventAckState ack_result);

  // Call once the main thread has handled an outstanding |type| event
  // in flight.
  void EventHandled(blink::WebInputEvent::Type type,
                    InputEventAckState ack_result);

  void set_is_flinging(bool is_flinging) { is_flinging_ = is_flinging; }

 private:
  friend class base::RefCountedThreadSafe<MainThreadEventQueue>;
  ~MainThreadEventQueue();
  void QueueEvent(std::unique_ptr<EventWithDispatchType> event);
  void SendEventNotificationToMainThread();
  void PopEventOnMainThread();
  void SendEventToMainThread(const blink::WebInputEvent* event,
                             const ui::LatencyInfo& latency,
                             InputEventDispatchType original_dispatch_type);

  friend class MainThreadEventQueueTest;
  int routing_id_;
  MainThreadEventQueueClient* client_;
  WebInputEventQueue<EventWithDispatchType> events_;
  std::unique_ptr<EventWithDispatchType> in_flight_event_;
  bool is_flinging_;

  base::Lock event_queue_lock_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadEventQueue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
