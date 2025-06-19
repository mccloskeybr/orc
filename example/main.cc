#include "src/workflow.h"

#include <string>
#include <iostream>

int main() {
  auto workflow = orc::Workflow<>()
    .Then([]() -> int { return 42; })
    .Then([](int x) -> double { return x * 0.5; })
    .Then([](double d) -> std::string { return "Value: " + std::to_string(d); });

  std::any result;
  workflow.run(result);
  std::cout << std::any_cast<std::string>(result) << '\n';  // Outputs: Value: 21.000000
}

