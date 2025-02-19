// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_manager.h"

#include <stddef.h>

#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/thread.h"
#include "base/trace_event/blame_context.h"
#include "base/trace_event/trace_buffer.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/real_time_domain.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate_for_test.h"
#include "components/scheduler/base/task_queue_selector.h"
#include "components/scheduler/base/test_count_uses_time_source.h"
#include "components/scheduler/base/test_task_time_tracker.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/base/work_queue.h"
#include "components/scheduler/base/work_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::_;
using scheduler::internal::EnqueueOrder;

namespace scheduler {

class MessageLoopTaskRunner : public TaskQueueManagerDelegateForTest {
 public:
  static scoped_refptr<MessageLoopTaskRunner> Create(
      std::unique_ptr<base::TickClock> tick_clock) {
    return make_scoped_refptr(new MessageLoopTaskRunner(std::move(tick_clock)));
  }

  // NestableTaskRunner implementation.
  bool IsNested() const override {
    return base::MessageLoop::current()->IsNested();
  }

 private:
  explicit MessageLoopTaskRunner(std::unique_ptr<base::TickClock> tick_clock)
      : TaskQueueManagerDelegateForTest(
            base::MessageLoop::current()->task_runner(),
            std::move(tick_clock)) {}
  ~MessageLoopTaskRunner() override {}
};

class TaskQueueManagerTest : public testing::Test {
 public:
  TaskQueueManagerTest() {}
  void DeleteTaskQueueManager() { manager_.reset(); }

 protected:
  void InitializeWithClock(size_t num_queues,
                           std::unique_ptr<base::TickClock> test_time_source) {
    test_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(now_src_.get(), false));
    main_task_runner_ = TaskQueueManagerDelegateForTest::Create(
        test_task_runner_.get(),
        base::WrapUnique(new TestTimeSource(now_src_.get())));

    manager_ = base::WrapUnique(new TaskQueueManager(
        main_task_runner_, "test.scheduler", "test.scheduler",
        "test.scheduler.debug"));
    manager_->SetTaskTimeTracker(&test_task_time_tracker_);

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));
  }

  void Initialize(size_t num_queues) {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    InitializeWithClock(num_queues,
                        base::WrapUnique(new TestTimeSource(now_src_.get())));
  }

  void InitializeWithRealMessageLoop(size_t num_queues) {
    now_src_.reset(new base::SimpleTestTickClock());
    message_loop_.reset(new base::MessageLoop());
    // A null clock triggers some assertions.
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    manager_ = base::WrapUnique(new TaskQueueManager(
        MessageLoopTaskRunner::Create(
            base::WrapUnique(new TestTimeSource(now_src_.get()))),
        "test.scheduler", "test.scheduler", "test.scheduler.debug"));
    manager_->SetTaskTimeTracker(&test_task_time_tracker_);

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));
  }

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<TaskQueueManagerDelegateForTest> main_task_runner_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner_;
  std::unique_ptr<TaskQueueManager> manager_;
  std::vector<scoped_refptr<internal::TaskQueueImpl>> runners_;
  TestTaskTimeTracker test_task_time_tracker_;
};

void PostFromNestedRunloop(base::MessageLoop* message_loop,
                           base::SingleThreadTaskRunner* runner,
                           std::vector<std::pair<base::Closure, bool>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  for (std::pair<base::Closure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->PostTask(FROM_HERE, pair.first);
    } else {
      runner->PostNonNestableTask(FROM_HERE, pair.first);
    }
  }
  base::RunLoop().RunUntilIdle();
}

void NopTask() {}

TEST_F(TaskQueueManagerTest,
       NowCalledMinimumNumberOfTimesToComputeTaskDurations) {
  message_loop_.reset(new base::MessageLoop());
  // This memory is managed by the TaskQueueManager, but we need to hold a
  // pointer to this object to read out how many times Now was called.
  TestCountUsesTimeSource* test_count_uses_time_source =
      new TestCountUsesTimeSource();

  manager_ = base::WrapUnique(new TaskQueueManager(
      MessageLoopTaskRunner::Create(
          base::WrapUnique(test_count_uses_time_source)),
      "test.scheduler", "test.scheduler", "test.scheduler.debug"));
  manager_->SetWorkBatchSize(6);
  manager_->SetTaskTimeTracker(&test_task_time_tracker_);

  for (size_t i = 0; i < 3; i++)
    runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));

  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&NopTask));

  base::RunLoop().RunUntilIdle();
  // We need to call Now for the beginning of the first task, and then the end
  // of every task after. We reuse the end time of one task for the start time
  // of the next task. In this case, there were 6 tasks, so we expect 7 calls to
  // Now.
  EXPECT_EQ(7, test_count_uses_time_source->now_calls_count());
}

TEST_F(TaskQueueManagerTest,
       NowNotCalledForNestedTasks) {
  message_loop_.reset(new base::MessageLoop());
  // This memory is managed by the TaskQueueManager, but we need to hold a
  // pointer to this object to read out how many times Now was called.
  TestCountUsesTimeSource* test_count_uses_time_source =
      new TestCountUsesTimeSource();

  manager_ = base::WrapUnique(new TaskQueueManager(
      MessageLoopTaskRunner::Create(
          base::WrapUnique(test_count_uses_time_source)),
      "test.scheduler", "test.scheduler", "test.scheduler.debug"));
  manager_->SetTaskTimeTracker(&test_task_time_tracker_);

  runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  for (int i = 0; i <= 6; ++i) {
    tasks_to_post_from_nested_loop.push_back(
        std::make_pair(base::Bind(&NopTask), true));
  }

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));

  base::RunLoop().RunUntilIdle();
  // We need to call Now twice, to measure the start and end of the outermost
  // task. We shouldn't call it for any of the nested tasks.
  EXPECT_EQ(2, test_count_uses_time_source->now_calls_count());
}

void NullTask() {}

void TestTask(EnqueueOrder value, std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(value);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 1, &run_order));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 5, &run_order));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 4, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 5, &run_order), true));

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));

  base::RunLoop().RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1, 2, 4, 5, 3));
}

TEST_F(TaskQueueManagerTest, QueuePolling) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  EXPECT_FALSE(runners_[0]->HasPendingImmediateWork());
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_TRUE(runners_[0]->HasPendingImmediateWork());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(runners_[0]->HasPendingImmediateWork());
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  EXPECT_EQ(delay, test_task_runner_->DelayToNextTaskTime());
  EXPECT_FALSE(runners_[0]->HasPendingImmediateWork());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
  EXPECT_FALSE(runners_[0]->HasPendingImmediateWork());
}

bool MessageLoopTaskCounter(size_t* count) {
  *count = *count + 1;
  return true;
}

TEST_F(TaskQueueManagerTest, DelayedTaskExecutedInOneMessageLoopTask) {
  Initialize(1u);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  test_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));
  EXPECT_EQ(1u, task_count);
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(8));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(3),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3, 2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_AscendingOrder) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(1));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(4),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1, 2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, PostDelayedTask_SharesUnderlyingDelayedTasks) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               delay);

  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
}

class TestObject {
 public:
  ~TestObject() { destructor_count_++; }

  void Run() { FAIL() << "TestObject::Run should not be called"; }

  static int destructor_count_;
};

int TestObject::destructor_count_ = 0;

TEST_F(TaskQueueManagerTest, PendingDelayedTasksRemovedOnShutdown) {
  Initialize(1u);

  TestObject::destructor_count_ = 0;

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())),
      delay);
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())));

  manager_.reset();

  EXPECT_EQ(2, TestObject::destructor_count_);
}

TEST_F(TaskQueueManagerTest, ManualPumping) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // However polling still works.
  EXPECT_TRUE(runners_[0]->HasPendingImmediateWork());

  // After pumping the task runs normally.
  LazyNow lazy_now(now_src_.get());
  runners_[0]->PumpQueue(&lazy_now, true);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingToggle) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // When pumping is enabled the task runs normally.
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_BeforePosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_AfterPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->SetQueueEnabled(false);

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_ManuallyPumpedTransitionsToAuto) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);
  runners_[0]->SetQueueEnabled(false);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  runners_[0]->SetQueueEnabled(true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithDelayedTask) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  // After pumping but before the delay period has expired, task does not run.
  LazyNow lazy_now1(now_src_.get());
  runners_[0]->PumpQueue(&lazy_now1, true);
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_TRUE(run_order.empty());

  // Once the delay has expired, pumping causes the task to run.
  now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  LazyNow lazy_now2(now_src_.get());
  runners_[0]->PumpQueue(&lazy_now2, true);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithMultipleDelayedTasks) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay1(base::TimeDelta::FromMilliseconds(1));
  base::TimeDelta delay2(base::TimeDelta::FromMilliseconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay1);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay2);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               delay3);

  now_src_->Advance(base::TimeDelta::FromMilliseconds(15));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Once the delay has expired, pumping causes the task to run.
  LazyNow lazy_now(now_src_.get());
  runners_[0]->PumpQueue(&lazy_now, true);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, DelayedTasksDontAutoRunWithManualPumping) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithNonEmptyWorkQueue) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  // Posting two tasks and pumping twice should result in two tasks in the work
  // queue.
  LazyNow lazy_now(now_src_.get());
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PumpQueue(&lazy_now, true);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PumpQueue(&lazy_now, true);

  EXPECT_EQ(2u, runners_[0]->immediate_work_queue()->Size());
}

void ReentrantTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int countdown,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(countdown);
  if (--countdown) {
    runner->PostTask(FROM_HERE,
                     Bind(&ReentrantTestTask, runner, countdown, out_result));
  }
}

TEST_F(TaskQueueManagerTest, ReentrantPosting) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE,
                        Bind(&ReentrantTestTask, runners_[0], 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_.reset();
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<base::SingleThreadTaskRunner> runner,
                      std::vector<EnqueueOrder>* run_order) {
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, run_order));
}

TEST_F(TaskQueueManagerTest, PostFromThread) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  base::Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runners_[0], &run_order));
  thread.Stop();

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

void RePostingTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int* run_count) {
  (*run_count)++;
  runner->PostTask(FROM_HERE, Bind(&RePostingTestTask,
                                   base::Unretained(runner.get()), run_count));
}

TEST_F(TaskQueueManagerTest, DoWorkCantPostItselfMultipleTimes) {
  Initialize(1u);

  int run_count = 0;
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybeScheduleDoWork there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(1, run_count);
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<EnqueueOrder> run_order;
  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 1, &run_order), true));

  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 0, &run_order));
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 1));
}

TEST_F(TaskQueueManagerTest, WorkBatching) {
  Initialize(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  // Running one task in the host message loop should cause two posted tasks to
  // get executed.
  EXPECT_EQ(test_task_runner_->NumPendingTasks(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  // The second task runs the remaining two posted tasks.
  EXPECT_EQ(test_task_runner_->NumPendingTasks(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeup) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Shouldn't run - no other task to wake TQM.

  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Still shouldn't wake TQM.

  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a task on an auto pumped queue should wake the TQM.
  EXPECT_THAT(run_order, ElementsAre(3, 1, 2));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupWhenAlreadyAwake) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1));  // TQM was already awake.
}

TEST_F(TaskQueueManagerTest,
       AutoPumpAfterWakeupTriggeredByManuallyPumpedQueue) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);
  runners_[1]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Shouldn't run - no other task to wake TQM.

  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  // This still shouldn't wake TQM as manual queue was not pumped.
  EXPECT_TRUE(run_order.empty());

  LazyNow lazy_now(now_src_.get());
  runners_[1]->PumpQueue(&lazy_now, true);
  test_task_runner_->RunUntilIdle();
  // Executing a task on an auto pumped queue should wake the TQM.
  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

void TestPostingTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::Closure task) {
  task_runner->PostTask(FROM_HERE, task);
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupFromTask) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<EnqueueOrder> run_order;
  // Check that a task which posts a task to an auto pump after wakeup queue
  // doesn't cause the queue to wake up.
  base::Closure after_wakeup_task = base::Bind(&TestTask, 1, &run_order);
  runners_[1]->PostTask(
      FROM_HERE, base::Bind(&TestPostingTask, runners_[0], after_wakeup_task));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Wake up the queue.
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupFromMultipleTasks) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<EnqueueOrder> run_order;
  // Check that a task which posts a task to an auto pump after wakeup queue
  // doesn't cause the queue to wake up.
  base::Closure after_wakeup_task_1 = base::Bind(&TestTask, 1, &run_order);
  base::Closure after_wakeup_task_2 = base::Bind(&TestTask, 2, &run_order);
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestPostingTask, runners_[0],
                                              after_wakeup_task_1));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestPostingTask, runners_[0],
                                              after_wakeup_task_2));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Wake up the queue.
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 1, 2));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupBecomesQuiescent) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  int run_count = 0;
  // Check that if multiple tasks reposts themselves onto a pump-after-wakeup
  // queue they don't wake each other and will eventually stop when no other
  // tasks execute.
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  // The reposting tasks posted to the after wakeup queue shouldn't have woken
  // each other up.
  EXPECT_EQ(2, run_count);
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupWithDontWakeQueue) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue0 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 0")
          .SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP));
  scoped_refptr<internal::TaskQueueImpl> queue1 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 0")
          .SetWakeupPolicy(TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES));
  scoped_refptr<internal::TaskQueueImpl> queue2 = runners_[0];

  std::vector<EnqueueOrder> run_order;
  queue0->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a DONT_WAKE_OTHER_QUEUES queue shouldn't wake the autopump after
  // wakeup queue.
  EXPECT_THAT(run_order, ElementsAre(2));

  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a CAN_WAKE_OTHER_QUEUES queue should wake the autopump after
  // wakeup queue.
  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

class MockTaskObserver : public base::MessageLoop::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask& task));
  MOCK_METHOD1(WillProcessTask, void(const base::PendingTask& task));
};

TEST_F(TaskQueueManagerTest, TaskObserverAdding) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(2);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(2);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, TaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);
  manager_->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  base::RunLoop().RunUntilIdle();
}

void RemoveObserverTask(TaskQueueManager* manager,
                        base::MessageLoop::TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, TaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(3);
  manager_->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RemoveObserverTask, manager_.get(), &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverAdding) {
  InitializeWithRealMessageLoop(2u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);
  runners_[0]->RemoveTaskObserver(&observer);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  base::RunLoop().RunUntilIdle();
}

void RemoveQueueObserverTask(scoped_refptr<TaskQueue> queue,
                             base::MessageLoop::TaskObserver* observer) {
  queue->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  runners_[0]->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RemoveQueueObserverTask, runners_[0], &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, ThreadCheckAfterTermination) {
  Initialize(1u);
  EXPECT_TRUE(runners_[0]->RunsTasksOnCurrentThread());
  manager_.reset();
  EXPECT_TRUE(runners_[0]->RunsTasksOnCurrentThread());
}

TEST_F(TaskQueueManagerTest, TimeDomain_NextScheduledRunTime) {
  Initialize(2u);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(10000));

  // With no delayed tasks.
  base::TimeTicks run_time;
  EXPECT_FALSE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));

  // With a non-delayed task.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  EXPECT_FALSE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));

  // With a delayed task.
  base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(50);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // With another delayed task in the same queue with a longer delay.
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(20);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay, run_time);

  // Test it updates as time progresses
  now_src_->Advance(expected_delay);
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks(), run_time);
}

TEST_F(TaskQueueManagerTest, TimeDomain_NextScheduledRunTime_MultipleQueues) {
  Initialize(3u);

  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(50);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay2);
  runners_[2]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay3);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));

  base::TimeTicks run_time;
  EXPECT_TRUE(manager_->real_time_domain()->NextScheduledRunTime(&run_time));
  EXPECT_EQ(now_src_->NowTicks() + delay2, run_time);
}

TEST_F(TaskQueueManagerTest, DeleteTaskQueueManagerInsideATask) {
  Initialize(1u);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&TaskQueueManagerTest::DeleteTaskQueueManager,
                            base::Unretained(this)));

  // This should not crash, assuming DoWork detects the TaskQueueManager has
  // been deleted.
  test_task_runner_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, GetAndClearSystemIsQuiescentBit) {
  Initialize(3u);

  scoped_refptr<internal::TaskQueueImpl> queue0 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 0").SetShouldMonitorQuiescence(true));
  scoped_refptr<internal::TaskQueueImpl> queue1 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 1").SetShouldMonitorQuiescence(true));
  scoped_refptr<internal::TaskQueueImpl> queue2 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 2").SetShouldMonitorQuiescence(false));

  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue1->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue2->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::Bind(&NopTask));
  queue1->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWork) {
  Initialize(2u);
  internal::TaskQueueImpl* queue0 = runners_[0].get();
  internal::TaskQueueImpl* queue1 = runners_[1].get();
  queue0->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  queue1->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue1->HasPendingImmediateWork());

  queue0->PostTask(FROM_HERE, base::Bind(NullTask));
  queue1->PostTask(FROM_HERE, base::Bind(NullTask));
  EXPECT_TRUE(queue0->HasPendingImmediateWork());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());

  LazyNow lazy_now(now_src_.get());
  queue1->PumpQueue(&lazy_now, true);
  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue1->HasPendingImmediateWork());
}

TEST_F(TaskQueueManagerTest, HasPendingImmediateWorkAndNeedsPumping) {
  Initialize(2u);
  internal::TaskQueueImpl* queue0 = runners_[0].get();
  internal::TaskQueueImpl* queue1 = runners_[1].get();
  queue0->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  queue1->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue0->NeedsPumping());
  EXPECT_FALSE(queue1->HasPendingImmediateWork());
  EXPECT_FALSE(queue1->NeedsPumping());

  queue0->PostTask(FROM_HERE, base::Bind(NullTask));
  queue0->PostTask(FROM_HERE, base::Bind(NullTask));
  queue1->PostTask(FROM_HERE, base::Bind(NullTask));
  EXPECT_TRUE(queue0->HasPendingImmediateWork());
  EXPECT_TRUE(queue0->NeedsPumping());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());
  EXPECT_TRUE(queue1->NeedsPumping());

  test_task_runner_->SetRunTaskLimit(1);
  test_task_runner_->RunPendingTasks();
  EXPECT_TRUE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue0->NeedsPumping());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());
  EXPECT_TRUE(queue1->NeedsPumping());

  test_task_runner_->ClearRunTaskLimit();
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue0->NeedsPumping());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());
  EXPECT_TRUE(queue1->NeedsPumping());

  LazyNow lazy_now(now_src_.get());
  queue1->PumpQueue(&lazy_now, true);
  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue0->NeedsPumping());
  EXPECT_TRUE(queue1->HasPendingImmediateWork());
  EXPECT_FALSE(queue1->NeedsPumping());

  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(queue0->HasPendingImmediateWork());
  EXPECT_FALSE(queue0->NeedsPumping());
  EXPECT_FALSE(queue1->HasPendingImmediateWork());
  EXPECT_FALSE(queue1->NeedsPumping());
}

void ExpensiveTestTask(int value,
                       base::SimpleTestTickClock* clock,
                       std::vector<EnqueueOrder>* out_result) {
  out_result->push_back(value);
  clock->Advance(base::TimeDelta::FromMilliseconds(1));
}

TEST_F(TaskQueueManagerTest, ImmediateAndDelayedTaskInterleaving) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  for (int i = 10; i < 19; i++) {
    runners_[0]->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExpensiveTestTask, i, now_src_.get(), &run_order),
        delay);
  }

  test_task_runner_->RunForPeriod(delay);

  for (int i = 0; i < 9; i++) {
    runners_[0]->PostTask(
        FROM_HERE,
        base::Bind(&ExpensiveTestTask, i, now_src_.get(), &run_order));
  }

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  // Delayed tasks are not allowed to starve out immediate work which is why
  // some of the immediate tasks run out of order.
  int expected_run_order[] = {
    10, 11, 12, 13, 0, 14, 15, 16, 1, 17, 18, 2, 3, 4, 5, 6, 7, 8
  };
  EXPECT_THAT(run_order, ElementsAreArray(expected_run_order));
}

TEST_F(TaskQueueManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_SameQueue) {
  Initialize(1u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  now_src_->Advance(delay * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest,
       DelayedTaskDoesNotSkipAHeadOfNonDelayedTask_DifferentQueues) {
  Initialize(2u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  now_src_->Advance(delay * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotSkipAHeadOfShorterDelayedTask) {
  Initialize(2u);

  std::vector<EnqueueOrder> run_order;
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay2);

  now_src_->Advance(delay1 * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

void CheckIsNested(bool* is_nested) {
  *is_nested = base::MessageLoop::current()->IsNested();
}

void PostAndQuitFromNestedRunloop(base::RunLoop* run_loop,
                                  base::SingleThreadTaskRunner* runner,
                                  bool* was_nested) {
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  runner->PostTask(FROM_HERE, run_loop->QuitClosure());
  runner->PostTask(FROM_HERE, base::Bind(&CheckIsNested, was_nested));
  run_loop->Run();
}

TEST_F(TaskQueueManagerTest, QuitWhileNested) {
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  InitializeWithRealMessageLoop(1u);
  manager_->SetWorkBatchSize(2);

  bool was_nested = true;
  base::RunLoop run_loop;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&PostAndQuitFromNestedRunloop,
                                              base::Unretained(&run_loop),
                                              base::RetainedRef(runners_[0]),
                                              base::Unretained(&was_nested)));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_nested);
}

class SequenceNumberCapturingTaskObserver
    : public base::MessageLoop::TaskObserver {
 public:
  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    sequence_numbers_.push_back(pending_task.sequence_num);
  }

  const std::vector<EnqueueOrder>& sequence_numbers() const {
    return sequence_numbers_;
  }

 private:
  std::vector<EnqueueOrder> sequence_numbers_;
};

TEST_F(TaskQueueManagerTest, SequenceNumSetWhenTaskIsPosted) {
  Initialize(1u);

  SequenceNumberCapturingTaskObserver observer;
  manager_->AddTaskObserver(&observer);

  // Register four tasks that will run in reverse order.
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(4, 3, 2, 1));

  // The sequence numbers are a zero-based monotonically incrememting counter
  // which should be set when the task is posted rather than when it's enqueued
  // onto the Incoming queue.
  EXPECT_THAT(observer.sequence_numbers(), ElementsAre(3, 2, 1, 0));

  manager_->RemoveTaskObserver(&observer);
}

TEST_F(TaskQueueManagerTest, NewTaskQueues) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec("foo"));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec("bar"));
  scoped_refptr<internal::TaskQueueImpl> queue3 =
      manager_->NewTaskQueue(TaskQueue::Spec("baz"));

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec("foo"));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec("bar"));
  scoped_refptr<internal::TaskQueueImpl> queue3 =
      manager_->NewTaskQueue(TaskQueue::Spec("baz"));

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<EnqueueOrder> run_order;
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  queue2->UnregisterTaskQueue();
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 3));
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue_WithDelayedTasks) {
  Initialize(2u);

  // Register three delayed tasks
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->UnregisterTaskQueue();
  test_task_runner_->RunUntilIdle();

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1, 3));
}

namespace {
void UnregisterQueue(scoped_refptr<internal::TaskQueueImpl> queue) {
  queue->UnregisterTaskQueue();
}
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue_InTasks) {
  Initialize(3u);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&UnregisterQueue, runners_[1]));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&UnregisterQueue, runners_[2]));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  ASSERT_THAT(run_order, ElementsAre(1));
}

void PostTestTasksFromNestedMessageLoop(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    scoped_refptr<base::SingleThreadTaskRunner> wake_up_runner,
    std::vector<EnqueueOrder>* run_order) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  main_runner->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 1, run_order));
  // The following should never get executed.
  wake_up_runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, run_order));
  base::RunLoop().RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, DeferredNonNestableTaskDoesNotTriggerWakeUp) {
  // This test checks that running (i.e., deferring) a non-nestable task in a
  // nested run loop does not trigger the pumping of an on-wakeup queue.
  InitializeWithRealMessageLoop(2u);
  runners_[1]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&PostTestTasksFromNestedMessageLoop, message_loop_.get(),
                 runners_[0], runners_[1], base::Unretained(&run_order)));

  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(run_order, ElementsAre(1));
}

namespace {

class MockObserver : public TaskQueueManager::Observer {
 public:
  MOCK_METHOD1(OnUnregisterTaskQueue,
               void(const scoped_refptr<TaskQueue>& queue));
  MOCK_METHOD2(OnTriedToExecuteBlockedTask,
               void(const TaskQueue& queue, const base::PendingTask& task));
};

}  // namespace

TEST_F(TaskQueueManagerTest, OnUnregisterTaskQueue) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec("test_queue"));

  EXPECT_CALL(observer, OnUnregisterTaskQueue(_)).Times(1);
  task_queue->UnregisterTaskQueue();

  manager_->SetObserver(nullptr);
}

TEST_F(TaskQueueManagerTest, OnTriedToExecuteBlockedTask) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue").SetShouldReportWhenExecutionBlocked(true));
  task_queue->SetQueueEnabled(false);
  task_queue->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_CALL(observer, OnTriedToExecuteBlockedTask(_, _)).Times(1);
  test_task_runner_->RunPendingTasks();

  manager_->SetObserver(nullptr);
}

TEST_F(TaskQueueManagerTest, ExecutedNonBlockedTask) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue").SetShouldReportWhenExecutionBlocked(true));
  task_queue->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_CALL(observer, OnTriedToExecuteBlockedTask(_, _)).Times(0);
  test_task_runner_->RunPendingTasks();

  manager_->SetObserver(nullptr);
}

void HasOneRefTask(std::vector<bool>* log, internal::TaskQueueImpl* tq) {
  log->push_back(tq->HasOneRef());
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueueInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  // We retain a reference to the task queue even when the manager has deleted
  // its reference.
  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec("test_queue"));

  std::vector<bool> log;
  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;

  // Inside a nested run loop, call task_queue->UnregisterTaskQueue, bookended
  // by calls to HasOneRefTask to make sure the manager doesn't release its
  // reference until the nested run loop exits.
  // NB: This first HasOneRefTask is a sanity check.
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&HasOneRefTask, base::Unretained(&log),
                                base::Unretained(task_queue.get())),
                     true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&internal::TaskQueueImpl::UnregisterTaskQueue,
                 base::Unretained(task_queue.get())), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&HasOneRefTask, base::Unretained(&log),
                                base::Unretained(task_queue.get())),
                     true));
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(),
                            base::RetainedRef(runners_[0]),
                            base::Unretained(&tasks_to_post_from_nested_loop)));
  base::RunLoop().RunUntilIdle();

  // Add a final call to HasOneRefTask.  This gives the manager a chance to
  // release its reference, and checks that it has.
  runners_[0]->PostTask(FROM_HERE,
                        base::Bind(&HasOneRefTask, base::Unretained(&log),
                                   base::Unretained(task_queue.get())));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(log, ElementsAre(false, false, true));
}

TEST_F(TaskQueueManagerTest, TimeDomainsAreIndependant) {
  Initialize(2u);

  base::TimeTicks start_time = manager_->delegate()->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(nullptr, start_time));
  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(nullptr, start_time));
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_a.get());
  runners_[1]->SetTimeDomain(domain_b.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  domain_b->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4, 5, 6));

  domain_a->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(4, 5, 6, 1, 2, 3));

  runners_[0]->UnregisterTaskQueue();
  runners_[1]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest, TimeDomainMigration) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->delegate()->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(nullptr, start_time));
  manager_->RegisterTimeDomain(domain_a.get());
  runners_[0]->SetTimeDomain(domain_a.get());

  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order),
                               base::TimeDelta::FromMilliseconds(40));

  domain_a->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(20));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(nullptr, start_time));
  manager_->RegisterTimeDomain(domain_b.get());
  runners_[0]->SetTimeDomain(domain_b.get());

  domain_b->AdvanceTo(start_time + base::TimeDelta::FromMilliseconds(50));
  manager_->MaybeScheduleImmediateWork(FROM_HERE);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));

  runners_[0]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

TEST_F(TaskQueueManagerTest, TimeDomainMigrationWithIncomingImmediateTasks) {
  Initialize(1u);

  base::TimeTicks start_time = manager_->delegate()->NowTicks();
  std::unique_ptr<VirtualTimeDomain> domain_a(
      new VirtualTimeDomain(nullptr, start_time));
  std::unique_ptr<VirtualTimeDomain> domain_b(
      new VirtualTimeDomain(nullptr, start_time));
  manager_->RegisterTimeDomain(domain_a.get());
  manager_->RegisterTimeDomain(domain_b.get());

  runners_[0]->SetTimeDomain(domain_a.get());
  std::vector<EnqueueOrder> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->SetTimeDomain(domain_b.get());

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));

  runners_[0]->UnregisterTaskQueue();

  manager_->UnregisterTimeDomain(domain_a.get());
  manager_->UnregisterTimeDomain(domain_b.get());
}

namespace {
void ChromiumRunloopInspectionTask(
    scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner) {
  EXPECT_EQ(1u, test_task_runner->NumPendingTasks());
}
}  // namespace

TEST_F(TaskQueueManagerTest, NumberOfPendingTasksOnChromiumRunLoop) {
  Initialize(1u);

  // NOTE because tasks posted to the chromiumrun loop are not cancellable, we
  // will end up with a lot more tasks posted if the delayed tasks were posted
  // in the reverse order.
  // TODO(alexclarke): Consider talking to the message pump directly.
  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  for (int i = 1; i < 100; i++) {
    runners_[0]->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromiumRunloopInspectionTask, test_task_runner_),
        base::TimeDelta::FromMilliseconds(i));
  }
  test_task_runner_->RunUntilIdle();
}

namespace {

class QuadraticTask {
 public:
  QuadraticTask(scoped_refptr<internal::TaskQueueImpl> task_queue,
                base::TimeDelta delay,
                base::SimpleTestTickClock* now_src)
      : count_(0), task_queue_(task_queue), delay_(delay), now_src_(now_src) {}

  void SetShouldExit(base::Callback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&QuadraticTask::Run, base::Unretained(this)),
        delay_);
    task_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&QuadraticTask::Run, base::Unretained(this)),
        delay_);
    now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  }

  int count() const { return count_; }

 private:
  int count_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
  base::TimeDelta delay_;
  base::Callback<bool()> should_exit_;
  base::SimpleTestTickClock* now_src_;
};

class LinearTask {
 public:
  LinearTask(scoped_refptr<internal::TaskQueueImpl> task_queue,
             base::TimeDelta delay,
             base::SimpleTestTickClock* now_src)
      : count_(0), task_queue_(task_queue), delay_(delay), now_src_(now_src) {}

  void SetShouldExit(base::Callback<bool()> should_exit) {
    should_exit_ = should_exit;
  }

  void Run() {
    if (should_exit_.Run())
      return;
    count_++;
    task_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&LinearTask::Run, base::Unretained(this)),
        delay_);
    now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  }

  int count() const { return count_; }

 private:
  int count_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
  base::TimeDelta delay_;
  base::Callback<bool()> should_exit_;
  base::SimpleTestTickClock* now_src_;
};

bool ShouldExit(QuadraticTask* quadratic_task, LinearTask* linear_task) {
  return quadratic_task->count() == 1000 || linear_task->count() == 1000;
}

}  // namespace

TEST_F(TaskQueueManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_SameQueue) {
  Initialize(1u);

  QuadraticTask quadratic_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  LinearTask linear_immediate_task(runners_[0], base::TimeDelta(),
                                   now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_immediate_task.count()) /
                 static_cast<double>(quadratic_delayed_task.count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_F(TaskQueueManagerTest, ImmediateWorkCanStarveDelayedTasks_SameQueue) {
  Initialize(1u);

  QuadraticTask quadratic_immediate_task(runners_[0], base::TimeDelta(),
                                         now_src_.get());
  LinearTask linear_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(&ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_delayed_task.count()) /
                 static_cast<double>(quadratic_immediate_task.count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_F(TaskQueueManagerTest,
       DelayedTasksDontBadlyStarveNonDelayedWork_DifferentQueue) {
  Initialize(2u);

  QuadraticTask quadratic_delayed_task(
      runners_[0], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  LinearTask linear_immediate_task(runners_[1], base::TimeDelta(),
                                   now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(ShouldExit, &quadratic_delayed_task, &linear_immediate_task);
  quadratic_delayed_task.SetShouldExit(should_exit);
  linear_immediate_task.SetShouldExit(should_exit);

  quadratic_delayed_task.Run();
  linear_immediate_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_immediate_task.count()) /
                 static_cast<double>(quadratic_delayed_task.count());

  EXPECT_GT(ratio, 0.333);
  EXPECT_LT(ratio, 1.1);
}

TEST_F(TaskQueueManagerTest,
       ImmediateWorkCanStarveDelayedTasks_DifferentQueue) {
  Initialize(2u);

  QuadraticTask quadratic_immediate_task(runners_[0], base::TimeDelta(),
                                         now_src_.get());
  LinearTask linear_delayed_task(
      runners_[1], base::TimeDelta::FromMilliseconds(10), now_src_.get());
  base::Callback<bool()> should_exit =
      base::Bind(&ShouldExit, &quadratic_immediate_task, &linear_delayed_task);

  quadratic_immediate_task.SetShouldExit(should_exit);
  linear_delayed_task.SetShouldExit(should_exit);

  quadratic_immediate_task.Run();
  linear_delayed_task.Run();

  test_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  test_task_runner_->RunUntilIdle();

  double ratio = static_cast<double>(linear_delayed_task.count()) /
                 static_cast<double>(quadratic_immediate_task.count());

  // This is by design, we want to enforce a strict ordering in task execution
  // where by delayed tasks can not skip ahead of non-delayed work.
  EXPECT_GT(ratio, 0.0);
  EXPECT_LT(ratio, 0.1);
}

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_NoTaskRunning) {
  Initialize(1u);

  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void CurrentlyExecutingTaskQueueTestTask(TaskQueueManager* task_queue_manager,
                                      std::vector<TaskQueue*>* task_sources) {
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());
}
}

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_TaskRunning) {
  Initialize(2u);

  internal::TaskQueueImpl* queue0 = runners_[0].get();
  internal::TaskQueueImpl* queue1 = runners_[1].get();

  std::vector<TaskQueue*> task_sources;
  queue0->PostTask(FROM_HERE, base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                         manager_.get(), &task_sources));
  queue1->PostTask(FROM_HERE, base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                         manager_.get(), &task_sources));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(task_sources, ElementsAre(queue0, queue1));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

namespace {
void RunloopCurrentlyExecutingTaskQueueTestTask(
    base::MessageLoop* message_loop,
    TaskQueueManager* task_queue_manager,
    std::vector<TaskQueue*>* task_sources,
    std::vector<std::pair<base::Closure, TaskQueue*>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());

  for (std::pair<base::Closure, TaskQueue*>& pair : *tasks) {
    pair.second->PostTask(FROM_HERE, pair.first);
  }

  base::RunLoop().RunUntilIdle();
  task_sources->push_back(task_queue_manager->currently_executing_task_queue());
}
}

TEST_F(TaskQueueManagerTest, CurrentlyExecutingTaskQueue_NestedLoop) {
  InitializeWithRealMessageLoop(3u);

  TaskQueue* queue0 = runners_[0].get();
  TaskQueue* queue1 = runners_[1].get();
  TaskQueue* queue2 = runners_[2].get();

  std::vector<TaskQueue*> task_sources;
  std::vector<std::pair<base::Closure, TaskQueue*>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                manager_.get(), &task_sources),
                     queue1));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&CurrentlyExecutingTaskQueueTestTask,
                                manager_.get(), &task_sources),
                     queue2));

  queue0->PostTask(
      FROM_HERE, base::Bind(&RunloopCurrentlyExecutingTaskQueueTestTask,
                            message_loop_.get(), manager_.get(), &task_sources,
                            &tasks_to_post_from_nested_loop));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(task_sources, ElementsAre(queue0, queue1, queue2, queue0));
  EXPECT_EQ(nullptr, manager_->currently_executing_task_queue());
}

void OnTraceDataCollected(base::Closure quit_closure,
                          base::trace_event::TraceResultBuffer* buffer,
                          const scoped_refptr<base::RefCountedString>& json,
                          bool has_more_events) {
  buffer->AddFragment(json->data());
  if (!has_more_events)
    quit_closure.Run();
}

class TaskQueueManagerTestWithTracing : public TaskQueueManagerTest {
 public:
  void StartTracing();
  void StopTracing();
  std::unique_ptr<trace_analyzer::TraceAnalyzer> CreateTraceAnalyzer();
};

void TaskQueueManagerTestWithTracing::StartTracing() {
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("*"),
      base::trace_event::TraceLog::RECORDING_MODE);
}

void TaskQueueManagerTestWithTracing::StopTracing() {
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

std::unique_ptr<trace_analyzer::TraceAnalyzer>
TaskQueueManagerTestWithTracing::CreateTraceAnalyzer() {
  base::trace_event::TraceResultBuffer buffer;
  base::trace_event::TraceResultBuffer::SimpleOutput trace_output;
  buffer.SetOutputCallback(trace_output.GetCallback());
  base::RunLoop run_loop;
  buffer.Start();
  base::trace_event::TraceLog::GetInstance()->Flush(
      Bind(&OnTraceDataCollected, run_loop.QuitClosure(),
           base::Unretained(&buffer)));
  run_loop.Run();
  buffer.Finish();

  return base::WrapUnique(
      trace_analyzer::TraceAnalyzer::Create(trace_output.json_output));
}

TEST_F(TaskQueueManagerTestWithTracing, BlameContextAttribution) {
  using trace_analyzer::Query;

  InitializeWithRealMessageLoop(1u);
  TaskQueue* queue = runners_[0].get();

  StartTracing();
  {
    base::trace_event::BlameContext blame_context("cat", "name", "type",
                                                  "scope", 0, nullptr);
    blame_context.Initialize();
    queue->SetBlameContext(&blame_context);
    queue->PostTask(FROM_HERE, base::Bind(&NopTask));
    base::RunLoop().RunUntilIdle();
  }
  StopTracing();
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer =
      CreateTraceAnalyzer();

  trace_analyzer::TraceEventVector events;
  Query q = Query::EventPhaseIs(TRACE_EVENT_PHASE_ENTER_CONTEXT) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_LEAVE_CONTEXT);
  analyzer->FindEvents(q, &events);

  EXPECT_EQ(2u, events.size());
}

}  // namespace scheduler
