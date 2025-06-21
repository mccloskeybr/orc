#ifndef ORC_H_
#define ORC_H_

#include <algorithm>
#include <any>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
// THREAD POOL
///////////////////////////////////////////////////////////////////////////////

class ThreadPool {
 public:
  explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency()) :
      threads_(),
      work_queue_(),
      work_queue_mutex_(),
      cv_(),
      terminate_(false) {
    threads_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; i++) {
      threads_.emplace_back(&ThreadPool::ThreadPoll, this);
    }
  }

  ~ThreadPool() {
    {
      std::scoped_lock lock(work_queue_mutex_);
      terminate_ = true;
    }
    cv_.notify_all();
    for (std::thread& thread : threads_) {
      thread.join();
    }
  }

  template <typename F, typename... Args>
  std::future<std::invoke_result_t<F, Args...>>
  Push(F&& fn, Args&&... args) {
    using RetType = std::invoke_result_t<F, Args...>;
    std::future<RetType> future;
    {
        std::scoped_lock lock(work_queue_mutex_);

        auto promise = std::make_shared<std::promise<RetType>>();
        future = promise->get_future();

        work_queue_.emplace([promise = std::move(promise),
                             fn = std::forward<F>(fn),
                             ... args = std::forward<Args>(args)]() {
          if constexpr (std::is_same<RetType, void>::value) {
              std::invoke(fn, args...);
              promise->set_value();
          } else {
              promise->set_value(std::invoke(fn, args...));
          }
        });
    }
    cv_.notify_one();
    return future;
  }

 private:
  void ThreadPoll() {
    while (true) {
      std::function<void(void)> work;
      {
        std::unique_lock<std::mutex> lock(work_queue_mutex_);
        cv_.wait(lock, [&]() { return terminate_ || !work_queue_.empty(); });
        if (terminate_ && work_queue_.empty()) { break; }
        work = work_queue_.front();
        work_queue_.pop();
      }
      work();
    }
  }

  std::vector<std::thread> threads_;
  std::queue<std::function<void(void)>> work_queue_;
  std::mutex work_queue_mutex_;
  std::condition_variable cv_;
  bool terminate_;
};

///////////////////////////////////////////////////////////////////////////////
// TRANSFORM
///////////////////////////////////////////////////////////////////////////////

class TransformBase {
  virtual std::any Invoke(std::any input) const = 0;
  friend class WorkflowBase;
};

template<typename Input, typename Output, typename... SideInputs>
class Transform : public TransformBase {
 public:
  template<typename... NewSideInputs>
  using With = Transform<Input, Output, SideInputs..., NewSideInputs...>;
  using InputType = Input;
  using OutputType = Output;

  static std::unique_ptr<Transform> Create(
      std::function<Output(Input, std::tuple<SideInputs...>)> closure,
      std::tuple<SideInputs...> side_inputs = {}) {
    return std::unique_ptr<Transform>(new Transform(closure, side_inputs));
  }

 protected:
  explicit Transform(
      std::function<Output(Input, std::tuple<SideInputs...>)> closure,
      std::tuple<SideInputs...> side_inputs) :
    closure_(closure),
    side_inputs_(side_inputs){}

  std::any Invoke(std::any input) const override final {
    return closure_(std::any_cast<Input>(input), side_inputs_);
  }

 private:
  std::function<Output(Input, std::tuple<SideInputs...>)> closure_;
  std::tuple<SideInputs...> side_inputs_;
};

///////////////////////////////////////////////////////////////////////////////
// WORKFLOW
///////////////////////////////////////////////////////////////////////////////

struct WorkflowOptions {
  size_t thread_count = std::thread::hardware_concurrency();
  size_t partition_size = 50;
};

class WorkflowBase {
 public:
  WorkflowBase(WorkflowOptions options) :
    options_(options) {
      assert(options_.thread_count > 0);
      assert(options_.partition_size > 0);
    }

  std::vector<std::any> Execute(std::span<std::any> input) {
    ThreadPool thread_pool(options_.thread_count);

    std::vector<std::future<std::vector<std::any>>> work_partitions;
    for (int i = 0; i < input.size(); i += options_.partition_size) {
      size_t partition_size = std::min(options_.partition_size, input.size() - i);
      auto start = input.begin() + i;
      std::vector<std::any> input_partition(start, start + partition_size);
      work_partitions.push_back(
          thread_pool.Push(&WorkflowBase::DoWork, this, std::move(input_partition)));
    }

    std::vector<std::any> result;
    result.reserve(input.size());
    for (auto& work_partition : work_partitions) {
      std::vector<std::any> partial_result = work_partition.get();
      result.insert(result.end(), partial_result.begin(), partial_result.end());
    }
    return result;
  }

 protected:
  std::vector<std::unique_ptr<TransformBase>> transforms_;
  WorkflowOptions options_;

 private:
  std::vector<std::any> DoWork(std::vector<std::any> input) {
    std::vector<std::any> current = std::move(input);
    for (int i = 0; i < current.size(); i++) {
      for (int j = 0; j < transforms_.size(); j++) {
        current[i] = transforms_[j].get()->Invoke(current[i]);
      }
    }
    return current;
  }
};

// NOTE: For subsequent Workflows.
template<typename CurrentTransform>
requires std::derived_from<CurrentTransform, TransformBase>
class WorkflowImpl : public WorkflowBase {
 public:
  template<typename NextTransform>
  requires std::derived_from<NextTransform, TransformBase>
  WorkflowImpl<NextTransform> Then(std::unique_ptr<NextTransform> transform) {
    using CurrentOutput = typename CurrentTransform::OutputType;
    using NextInput = typename NextTransform::InputType;
    static_assert(
        std::is_same_v<CurrentOutput, NextInput>,
        "Input type must match output type of previous transform.");
    this->transforms_.push_back(std::move(transform));
    return WorkflowImpl<NextTransform>(std::move(*this));
  }
};

// NOTE: For first defined Workflow.
class Workflow : public WorkflowBase {
 public:
  template<typename NextTransform>
  requires std::derived_from<NextTransform, TransformBase>
  WorkflowImpl<NextTransform> Then(std::unique_ptr<NextTransform> transform) {
    this->transforms_.push_back(std::move(transform));
    return WorkflowImpl<NextTransform>(std::move(*this));
  }
};

#endif
