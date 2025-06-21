#include <string>
#include <iostream>

#include "orc.h"

int main() {
  WorkflowOptions opts {
    .thread_count = 12,
    .partition_size = 500,
  };
  auto workflow = Workflow(opts)
    .Then(Transform<int, int>::With<int, int>::Create(
          +[](int x, std::tuple<int, int> side_inputs) -> int {
            return x + std::get<0>(side_inputs) + std::get<1>(side_inputs);
          }, std::make_tuple(5, 10)))
    .Then(Transform<int, int>::Create(
          +[](int x, std::tuple<> side_inputs) -> int {
            return x * 10;
          }))
    .Then(Transform<int, std::string>::Create(
          +[](int x, std::tuple<> side_inputs) -> std::string {
            return "Final result: " + std::to_string(x);
          }));

    std::vector<std::any> input;
    for (int32_t i = 0; i < 100; i++) { input.push_back(i); }
    std::vector<std::any> result = workflow.Execute(input);
    for (std::any& r : result) {
      std::cout << std::any_cast<std::string>(r) << std::endl;
    }
}

