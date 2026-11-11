#include <cstring>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

// C Convention
void TakeCharStar(const char *s) {
  std::cout << s; // Relies on '\0' to know where to stop!
}

// Old Standard C++ convention
void TakeString(const std::string &s) { std::cout << s; }

// C++17++ string_view
// accepts char*, std::string with zero copy
void TakeStringView(std::string_view s) { std::cout << s; }

void AlreadyHasString_c(const std::string &s) {
  // Callers needing to convert a std::string to a const char*
  // must use the explicit c_str() function:
  TakeCharStar(s.c_str());
}

void AlreadyHasString_s(const std::string &s) {
  TakeStringView(s); // No explicit conversion; convenient!
}

void AlreadyHasCharStar(const char *s) {
  TakeStringView(s); // GOOD: No copy; efficient!
  TakeString(s);     // BAD: Compiler will construct a temporary std::string
                     // (potential heap allocation)
}

int main() {
  char characters[10] = "asdf";
  char name[10] = {'J', 'o', 'h', 'n', 'y', 'D', 'e', 'p', 'p'};
  int count = 5;

  char s1[10] = "hello";    // Stack-allocated array
  const char *s2 = "hello"; // Pointer to read-only string literal
  // with C++98 to C++11++
  std::string ss = "hello";
  ss += " world"; // Automatic memory management!

  // with C++17++ string view safety
  TakeStringView(characters);
  std::cout << '\n';

  std::string msg = std::format("Hello, {}! You have {} new msg.", name, count);
  std::print("Hello, {}!\n", name);

  char buffer[5]; // Can hold 4 chars + 1 null terminator
  // BAD: "hello" is 5 chars + '\0' = 6 bytes total!
  std::strcpy(buffer, "hello"); // Writes past the end of 'buffer'. Undefined
                                // behavior / Stack corruption!
  std::print("buffer: {}", buffer);

  std::string s = "Hello Low Latency";
  const char *raw_ptr = s.c_str(); // Points to s's internal buffer

  // Modifying the string triggers a memory reallocation on the heap
  s += " and high performance trading systems!";

  // BAD: raw_ptr now points to freed/reallocated memory!
  std::cout << raw_ptr;
  return 0;
}