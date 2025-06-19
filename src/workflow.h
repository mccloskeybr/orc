#ifndef SRC_TRANSFORM_H_
#define SRC_TRANSFORM_H_

#include <any>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace orc {

template<typename T>
struct FunctionTraits;

// Zero arg
template<typename R>
struct FunctionTraits<R(*)()> {
  using OutputType = R;
  static constexpr std::size_t arity = 0;
};

// Single arg
template<typename R, typename Arg>
struct FunctionTraits<R(*)(Arg)> {
  using InputType = Arg;
  using OutputType = R;
  static constexpr std::size_t arity = 1;
};

template<typename Func>
concept NonCapturingNullaryCallable =
  requires {
    typename FunctionTraits<decltype(+std::declval<Func>())>;
  } &&
  FunctionTraits<decltype(+std::declval<Func>())>::arity == 0;

template<typename Func>
concept NonCapturingUnaryCallable =
  requires {
    typename FunctionTraits<decltype(+std::declval<Func>())>;
  } &&
  FunctionTraits<decltype(+std::declval<Func>())>::arity == 1;

class WorkflowBase {
 public:
  void run(std::any& input) {
    for (auto& step : steps) {
      step(input);
    }
  }

  using AnyFunction = std::function<void(std::any&)>;
  std::vector<AnyFunction> steps;
};

template<typename CurrentType = void>
class Workflow : public WorkflowBase {
 public:
  Workflow() = default;

  template<typename OtherType>
  explicit Workflow(Workflow<OtherType>&& other) {
    this->steps = std::move(other.steps);
  }

  template<NonCapturingNullaryCallable Func>
  requires std::is_same_v<CurrentType, void>
  auto Then(Func closure) {
    using FnPtr = decltype(+closure);
    using Traits = FunctionTraits<FnPtr>;
    using Output = typename Traits::OutputType;

    this->steps.emplace_back([closure](std::any& input) {
      Output result = closure();
      input = std::any(std::move(result));
    });

    return Workflow<Output>(std::move(*this));
  }

  template<NonCapturingUnaryCallable Func>
  requires (!std::is_same_v<CurrentType, void>)
  auto Then(Func closure) {
    using FnPtr = decltype(+closure);
    using Traits = FunctionTraits<FnPtr>;
    using Input = typename Traits::InputType;
    using Output = typename Traits::OutputType;

    static_assert(std::is_same_v<Input, CurrentType>,
                  "Input type must match output of previous closure.");

    this->steps.emplace_back([closure](std::any& input) {
      auto& typed_input = *std::any_cast<Input>(&input);
      Output result = closure(typed_input);
      input = std::any(std::move(result));
    });

    return Workflow<Output>(std::move(*this));
  }
};

}
#endif
