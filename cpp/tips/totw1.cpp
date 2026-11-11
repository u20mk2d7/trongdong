#include "llvm/Bitcode/BitcodeAnalyzer.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

// C Convention
void TakesCharStar(const char *s) {
  std::cout << s; // Relies on '\0' to know where to stop!
}

// Old Standard C++ convention
void TakesString(const std::string &s) { std::cout << s; }

// C++17++ string_view
// accepts char*, std::string with zero copy
void TakesStringView(std::string_view s) { std::cout << s; }

void AlreadyHasString_c(const std::string &s) {
  // Callers needing to convert a std::string to a const char*
  // must use the explicit c_str() function:
  TakesCharStar(s.c_str());
}

void AlreadyHasString_s(const std::string &s) {
  TakesStringView(s); // No explicit conversion; convenient!
}

void AlreadyHasCharStar(const char *s) {
  TakesStringView(s); // GOOD: No copy; efficient!
  TakesString(s);     // BAD: Compiler will construct a temporary std::string
                      // (potential heap allocation)
}

int main() {
  char characters[10] = "asdf";
  char name[10] = {'J', 'o', 'h', 'n', 'y', 'D', 'e', 'p', 'p'};
  int count = 5;

  // with C++17++ string view safety
  TakesStringView(characters);
  std::cout << '\n';

  return 0;
}