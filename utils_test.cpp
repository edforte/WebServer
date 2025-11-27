#include "utils.hpp"

#include <fcntl.h>  // fcntl, F_GETFL, O_NONBLOCK
#include <gtest/gtest.h>
#include <unistd.h>  // pipe, close

#include <set>
#include <string>

TEST(SetNonblockingTests, InvalidFdReturnsMinusOne) {
  EXPECT_EQ(set_nonblocking(-1), -1);
}

TEST(SetNonblockingTests, SetsNonblockingOnPipeFdAndIsIdempotent) {
  int fds[2];
  ASSERT_EQ(pipe(fds), 0) << "pipe() failed";
  int readFd = fds[0];
  int writeFd = fds[1];

  int flags_before = fcntl(readFd, F_GETFL, 0);
  ASSERT_GE(flags_before, 0) << "F_GETFL failed on pipe fd";

  int ret = set_nonblocking(readFd);
  EXPECT_GE(ret, 0) << "set_nonblocking failed";

  int flags_after = fcntl(readFd, F_GETFL, 0);
  ASSERT_GE(flags_after, 0);
  EXPECT_TRUE((flags_after & O_NONBLOCK) != 0)
      << "O_NONBLOCK not set after call";

  int ret2 = set_nonblocking(readFd);
  EXPECT_GE(ret2, 0);

  close(readFd);
  close(writeFd);
}

TEST(TrimCopyTests, EmptyString) {
  EXPECT_EQ(trim_copy(""), "");
}

TEST(TrimCopyTests, AllWhitespaceBecomesEmpty) {
  EXPECT_EQ(trim_copy("    \t\n  \r "), "");
}

TEST(TrimCopyTests, LeadingWhitespaceRemoved) {
  EXPECT_EQ(trim_copy("   hello"), "hello");
}

TEST(TrimCopyTests, TrailingWhitespaceRemoved) {
  EXPECT_EQ(trim_copy("world   \n\t"), "world");
}

TEST(TrimCopyTests, BothEndsTrimmedAndInternalPreserved) {
  EXPECT_EQ(trim_copy("  hello   world  "), "hello   world");
}

TEST(TrimCopyTests, MixedWhitespaceCharactersTrimmed) {
  std::string str = "\t\n  example\t\n";
  EXPECT_EQ(trim_copy(str), "example");
}

TEST(InitDefaultHttpMethodsTests, InsertsFiveStandardMethods) {
  std::set<http::Method> methods;
  initDefaultHttpMethods(methods);

  EXPECT_EQ(methods.size(), 5U);

  EXPECT_NE(methods.find(http::GET), methods.end());
  EXPECT_NE(methods.find(http::POST), methods.end());
  EXPECT_NE(methods.find(http::PUT), methods.end());
  EXPECT_NE(methods.find(http::DELETE), methods.end());
  EXPECT_NE(methods.find(http::HEAD), methods.end());
}

TEST(SafeStrtollTests, ParsesValidPositiveNumber) {
  long long result;
  EXPECT_TRUE(safeStrtoll("12345", result));
  EXPECT_EQ(result, 12345);
}

TEST(SafeStrtollTests, ParsesValidNegativeNumber) {
  long long result;
  EXPECT_TRUE(safeStrtoll("-42", result));
  EXPECT_EQ(result, -42);
}

TEST(SafeStrtollTests, ParsesZero) {
  long long result;
  EXPECT_TRUE(safeStrtoll("0", result));
  EXPECT_EQ(result, 0);
}

TEST(SafeStrtollTests, FailsOnEmptyString) {
  long long result;
  EXPECT_FALSE(safeStrtoll("", result));
}

TEST(SafeStrtollTests, FailsOnNonNumericString) {
  long long result;
  EXPECT_FALSE(safeStrtoll("abc", result));
}

TEST(SafeStrtollTests, FailsOnMixedContent) {
  long long result;
  EXPECT_FALSE(safeStrtoll("123abc", result));
}

TEST(SafeStrtollTests, FailsOnTrailingSpaces) {
  long long result;
  EXPECT_FALSE(safeStrtoll("123 ", result));
}

TEST(SafeStrtollTests, HandlesLeadingSpaces) {
  // Note: strtoll accepts leading whitespace, which matches atoll behavior.
  // This test verifies that behavior is preserved.
  long long result;
  EXPECT_TRUE(safeStrtoll(" 123", result));
  EXPECT_EQ(result, 123);
}
