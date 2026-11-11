#include <cstring>
#include <format>
#include <iostream>
#include <print>
#include <string>
#include <string_view>

// 1. The Buffer Overflow
void bufferOverflow() {
  char buffer[5]; // Can hold 4 chars + 1 null terminator
  // BAD: "hello" is 5 chars + '\0' = 6 bytes total!
  std::strcpy(buffer, "hello");
}

int main() { return 0; }