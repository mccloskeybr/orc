# Orc

Very simple workflow orchestration / management framework, comparable to
Apache Beam and similar.

The basic goal is to provide a framework to facilitate writing multithreaded
code. That is, given some input, apply some uniform set of transformations on
every element in that input to produce some output. Orc provides a framework
that allows these transformations to be written in a per-element fashion, which
is then easily multithreaded across every element in the input.

Consider this example program:

```
#include "orc.h"

int main() {
  // High level workflow options. Controls e.g. number of threads.
  WorkflowOptions opts;

  // A workflow describes a series of transformations to apply to some uniform input.
  // Stages are chained together using `Then(...).` This is a simple workflow
  // that, given an input, adds 5 to it, stringifies it, and prefixes it with "Result: ".
  auto workflow = Workflow(opts)
    // Each transformation has two template parameters, `<InputType, OutputType>`.
    // Static side inputs can be ingested with `::With<SideInputTypes...>`.
    .Then(Transform<int, int>::With<int>::Create(
          // A function pointer is passed, matching the input / output format of the
          // transform templates. A tuple is also passed as a parameter, matching the
          // side inputs types.
          +[](int x, std::tuple<int> side_inputs) -> int {
            return x + std::get<0>(side_inputs);
          }, std::make_tuple(5)))
    // Orc statically checks that the Input of one stage logically follows the Output
    // of the previous.
    .Then(Transform<int, std::string>::Create(
          +[](int x, std::tuple<> side_inputs) -> std::string {
            return "Result: " + std::to_string(x);
          }));

    std::vector<std::any> input;
    for (int32_t i = 0; i < 100; i++) { input.push_back(i); }
    // workflow.Execute applies the described transformations on every element
    // in the input collection.
    std::vector<std::any> result = workflow.Execute(input);
}
```

