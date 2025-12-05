#include "Uri.hpp"

#include <gtest/gtest.h>

using namespace http;

// ==================== PARSING TESTS ====================

TEST(UriParseTests, SimpleAbsolutePath) {
  Uri uri("/path/to/resource");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPath(), "/path/to/resource");
  EXPECT_EQ(uri.getQuery(), "");
  EXPECT_EQ(uri.getFragment(), "");
}

TEST(UriParseTests, PathWithQueryString) {
  Uri uri("/search?q=hello&page=1");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPath(), "/search");
  EXPECT_EQ(uri.getQuery(), "q=hello&page=1");
}

TEST(UriParseTests, PathWithFragment) {
  Uri uri("/page#section1");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPath(), "/page");
  EXPECT_EQ(uri.getFragment(), "section1");
}

TEST(UriParseTests, PathWithQueryAndFragment) {
  Uri uri("/page?id=5#top");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPath(), "/page");
  EXPECT_EQ(uri.getQuery(), "id=5");
  EXPECT_EQ(uri.getFragment(), "top");
}

TEST(UriParseTests, FragmentInQueryString) {
  Uri uri("/path?foo=bar#anchor");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPath(), "/path");
  EXPECT_EQ(uri.getQuery(), "foo=bar");
  EXPECT_EQ(uri.getFragment(), "anchor");
}

TEST(UriParseTests, FullUrl) {
  Uri uri("http://example.com:8080/path?query=1#frag");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getScheme(), "http");
  EXPECT_EQ(uri.getHost(), "example.com");
  EXPECT_EQ(uri.getPort(), 8080);
  EXPECT_EQ(uri.getPath(), "/path");
  EXPECT_EQ(uri.getQuery(), "query=1");
  EXPECT_EQ(uri.getFragment(), "frag");
}

TEST(UriParseTests, UrlWithoutPort) {
  Uri uri("https://example.com/resource");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getScheme(), "https");
  EXPECT_EQ(uri.getHost(), "example.com");
  EXPECT_EQ(uri.getPort(), -1);
  EXPECT_EQ(uri.getPath(), "/resource");
}

TEST(UriParseTests, EmptyUrl) {
  Uri uri("");
  EXPECT_FALSE(uri.isValid());
}

TEST(UriParseTests, RootPath) {
  Uri uri("/");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPath(), "/");
}

// ==================== PORT VALIDATION TESTS ====================

TEST(UriParseTests, EmptyPortString) {
  Uri uri("http://example.com:/path");
  EXPECT_FALSE(uri.isValid());
}

TEST(UriParseTests, InvalidPortWithNonDigits) {
  Uri uri("http://example.com:abc/path");
  EXPECT_FALSE(uri.isValid());
}

TEST(UriParseTests, PortOverflow) {
  Uri uri("http://example.com:999999999999999999999/path");
  EXPECT_FALSE(uri.isValid());
}

TEST(UriParseTests, PortOutOfValidRange) {
  Uri uri("http://example.com:99999/path");
  EXPECT_FALSE(uri.isValid());
}

TEST(UriParseTests, ValidPortAtMaxRange) {
  Uri uri("http://example.com:65535/path");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPort(), 65535);
}

TEST(UriParseTests, InvalidPortZero) {
  Uri uri("http://example.com:0/path");
  EXPECT_FALSE(uri.isValid());
}

TEST(UriParseTests, ValidPortAtMinRange) {
  Uri uri("http://example.com:1/path");
  EXPECT_TRUE(uri.isValid());
  EXPECT_EQ(uri.getPort(), 1);
}

// ==================== URL DECODING TESTS ====================

TEST(UriDecodeTests, NoEncoding) {
  EXPECT_EQ(Uri::decode("hello"), "hello");
}

TEST(UriDecodeTests, SpaceAsPlus) {
  EXPECT_EQ(Uri::decode("hello+world"), "hello world");
}

TEST(UriDecodeTests, PercentEncodedSpace) {
  EXPECT_EQ(Uri::decode("hello%20world"), "hello world");
}

TEST(UriDecodeTests, PercentEncodedDot) {
  EXPECT_EQ(Uri::decode("%2e"), ".");
  EXPECT_EQ(Uri::decode("%2E"), ".");
}

TEST(UriDecodeTests, PercentEncodedDoubleDot) {
  EXPECT_EQ(Uri::decode("%2e%2e"), "..");
  EXPECT_EQ(Uri::decode("%2E%2E"), "..");
  EXPECT_EQ(Uri::decode("%2e%2E"), "..");
  EXPECT_EQ(Uri::decode("%2E%2e"), "..");
}

TEST(UriDecodeTests, MixedEncoding) {
  EXPECT_EQ(Uri::decode("/path%2Fto%2Fresource"), "/path/to/resource");
}

TEST(UriDecodeTests, InvalidPercentSequence) {
  // Invalid hex characters - should pass through unchanged
  EXPECT_EQ(Uri::decode("%GG"), "%GG");
  EXPECT_EQ(Uri::decode("%2"), "%2");
}

TEST(UriDecodeTests, SpecialCharacters) {
  EXPECT_EQ(Uri::decode("%21"), "!");
  EXPECT_EQ(Uri::decode("%40"), "@");
  EXPECT_EQ(Uri::decode("%23"), "#");
}

// ==================== PATH DECODING TESTS ====================

TEST(UriDecodePathTests, NoEncoding) {
  EXPECT_EQ(Uri::decodePath("hello"), "hello");
}

TEST(UriDecodePathTests, PlusStaysAsPlus) {
  // In paths, '+' should remain as '+', not be converted to space
  EXPECT_EQ(Uri::decodePath("hello+world"), "hello+world");
  EXPECT_EQ(Uri::decodePath("/path/file+name.txt"), "/path/file+name.txt");
  EXPECT_EQ(Uri::decodePath("/c++/tutorial"), "/c++/tutorial");
}

TEST(UriDecodePathTests, PercentEncodedSpace) {
  EXPECT_EQ(Uri::decodePath("hello%20world"), "hello world");
  EXPECT_EQ(Uri::decodePath("/path%20to%20file"), "/path to file");
}

TEST(UriDecodePathTests, PercentEncodedPlus) {
  // Percent-encoded plus should decode to '+'
  EXPECT_EQ(Uri::decodePath("hello%2Bworld"), "hello+world");
}

TEST(UriDecodePathTests, PercentEncodedDot) {
  EXPECT_EQ(Uri::decodePath("%2e"), ".");
  EXPECT_EQ(Uri::decodePath("%2E"), ".");
}

TEST(UriDecodePathTests, PercentEncodedDoubleDot) {
  EXPECT_EQ(Uri::decodePath("%2e%2e"), "..");
  EXPECT_EQ(Uri::decodePath("%2E%2E"), "..");
}

TEST(UriDecodePathTests, MixedEncoding) {
  EXPECT_EQ(Uri::decodePath("/path%2Fto%2Fresource"), "/path/to/resource");
}

TEST(UriDecodePathTests, InvalidPercentSequence) {
  EXPECT_EQ(Uri::decodePath("%GG"), "%GG");
  EXPECT_EQ(Uri::decodePath("%2"), "%2");
}

TEST(UriDecodePathTests, SpecialCharacters) {
  EXPECT_EQ(Uri::decodePath("%21"), "!");
  EXPECT_EQ(Uri::decodePath("%40"), "@");
  EXPECT_EQ(Uri::decodePath("%23"), "#");
}

// ==================== QUERY STRING DECODING TESTS ====================

TEST(UriDecodeQueryTests, NoEncoding) {
  EXPECT_EQ(Uri::decodeQuery("hello"), "hello");
}

TEST(UriDecodeQueryTests, PlusAsSpace) {
  // In query strings, '+' should be converted to space
  EXPECT_EQ(Uri::decodeQuery("hello+world"), "hello world");
  EXPECT_EQ(Uri::decodeQuery("first+name"), "first name");
  EXPECT_EQ(Uri::decodeQuery("a+b+c"), "a b c");
}

TEST(UriDecodeQueryTests, PercentEncodedSpace) {
  EXPECT_EQ(Uri::decodeQuery("hello%20world"), "hello world");
}

TEST(UriDecodeQueryTests, PercentEncodedPlus) {
  // Percent-encoded plus should decode to '+'
  EXPECT_EQ(Uri::decodeQuery("one%2Btwo"), "one+two");
}

TEST(UriDecodeQueryTests, MixedSpaceEncoding) {
  // Both '+' and '%20' should decode to space
  EXPECT_EQ(Uri::decodeQuery("hello+world%20test"), "hello world test");
}

TEST(UriDecodeQueryTests, PercentEncodedSpecialChars) {
  EXPECT_EQ(Uri::decodeQuery("key%3Dvalue"), "key=value");
  EXPECT_EQ(Uri::decodeQuery("a%26b"), "a&b");
}

TEST(UriDecodeQueryTests, InvalidPercentSequence) {
  EXPECT_EQ(Uri::decodeQuery("%GG"), "%GG");
  EXPECT_EQ(Uri::decodeQuery("%2"), "%2");
}

// ==================== URL ENCODING TESTS ====================

TEST(UriEncodeTests, NoEncodingNeeded) {
  EXPECT_EQ(Uri::encode("hello"), "hello");
  EXPECT_EQ(Uri::encode("Hello-World_123.txt"), "Hello-World_123.txt");
}

TEST(UriEncodeTests, SpaceEncoded) {
  EXPECT_EQ(Uri::encode("hello world"), "hello%20world");
}

TEST(UriEncodeTests, SpecialCharacters) {
  EXPECT_EQ(Uri::encode("a/b"), "a%2Fb");
  EXPECT_EQ(Uri::encode("a?b"), "a%3Fb");
  EXPECT_EQ(Uri::encode("a#b"), "a%23b");
}

TEST(UriEncodeTests, NonAscii) {
  // UTF-8 encoded string
  EXPECT_EQ(Uri::encode("\xC3\xA9"), "%C3%A9");  // é in UTF-8
}

// ==================== PATH TRAVERSAL DETECTION TESTS ====================

TEST(UriPathTraversalTests, NoDotDot) {
  Uri uri("/path/to/file");
  EXPECT_FALSE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, SimpleDotDot) {
  Uri uri("/path/../secret");
  EXPECT_TRUE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, DotDotAtStart) {
  Uri uri("/../etc/passwd");
  EXPECT_TRUE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, DotDotAtEnd) {
  Uri uri("/path/to/..");
  EXPECT_TRUE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, EncodedDotDotLowercase) {
  Uri uri("/path/%2e%2e/secret");
  EXPECT_TRUE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, EncodedDotDotUppercase) {
  Uri uri("/path/%2E%2E/secret");
  EXPECT_TRUE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, EncodedDotDotMixed) {
  Uri uri("/path/%2e%2E/secret");
  EXPECT_TRUE(uri.hasPathTraversal());
}

TEST(UriPathTraversalTests, SingleDot) {
  Uri uri("/path/./file");
  EXPECT_FALSE(uri.hasPathTraversal());  // Single dot is not traversal
}

TEST(UriPathTraversalTests, TripleDot) {
  Uri uri("/path/.../file");
  EXPECT_FALSE(uri.hasPathTraversal());  // "..." is not traversal
}

TEST(UriPathTraversalTests, DotDotInFilename) {
  Uri uri("/path/file..txt");
  EXPECT_FALSE(uri.hasPathTraversal());  // ".." in filename is OK
}

// ==================== PATH NORMALIZATION TESTS ====================

TEST(UrlNormalizeTests, AlreadyNormalized) {
  EXPECT_EQ(Uri::normalizePath("/a/b/c"), "/a/b/c");
}

TEST(UrlNormalizeTests, SingleDots) {
  EXPECT_EQ(Uri::normalizePath("/a/./b/./c"), "/a/b/c");
}

TEST(UrlNormalizeTests, DoubleDots) {
  EXPECT_EQ(Uri::normalizePath("/a/b/../c"), "/a/c");
}

TEST(UrlNormalizeTests, MultipleDoubleDots) {
  EXPECT_EQ(Uri::normalizePath("/a/b/c/../../d"), "/a/d");
}

TEST(UrlNormalizeTests, DoubleDotAtStart) {
  // Can't go above root
  EXPECT_EQ(Uri::normalizePath("/../a"), "/a");
}

TEST(UrlNormalizeTests, EncodedPath) {
  EXPECT_EQ(Uri::normalizePath("/a/%2e%2e/b"), "/b");
}

TEST(UrlNormalizeTests, EmptyPath) {
  EXPECT_EQ(Uri::normalizePath(""), "/");
}

TEST(UrlNormalizeTests, RootPath) {
  EXPECT_EQ(Uri::normalizePath("/"), "/");
}

TEST(UrlNormalizeTests, TrailingSlashPreserved) {
  EXPECT_EQ(Uri::normalizePath("/a/b/"), "/a/b/");
}

TEST(UrlNormalizeTests, TrailingSlashWithDoubleDots) {
  EXPECT_EQ(Uri::normalizePath("/a/b/../c/"), "/a/c/");
}

// ==================== SERIALIZATION TESTS ====================

TEST(UrlSerializeTests, SimplePath) {
  Uri uri("/path/to/file");
  EXPECT_EQ(uri.serialize(), "/path/to/file");
}

TEST(UrlSerializeTests, PathWithQuery) {
  Uri uri("/search?q=test");
  EXPECT_EQ(uri.serialize(), "/search?q=test");
}

TEST(UrlSerializeTests, PathWithQueryAndFragment) {
  Uri uri("/page?id=1#top");
  EXPECT_EQ(uri.serialize(), "/page?id=1#top");
}

TEST(UrlSerializeTests, FullUrl) {
  Uri uri("http://example.com:8080/path?q=1#f");
  EXPECT_EQ(uri.serialize(), "http://example.com:8080/path?q=1#f");
}

// ==================== DECODED PATH TESTS ====================

TEST(UrlDecodedPathTests, NoEncoding) {
  Uri uri("/path/to/file");
  EXPECT_EQ(uri.getDecodedPath(), "/path/to/file");
}

TEST(UrlDecodedPathTests, EncodedSpaces) {
  Uri uri("/path%20to%20file");
  EXPECT_EQ(uri.getDecodedPath(), "/path to file");
}

TEST(UrlDecodedPathTests, EncodedSlash) {
  Uri uri("/path%2Fto%2Ffile");
  EXPECT_EQ(uri.getDecodedPath(), "/path/to/file");
}

TEST(UrlDecodedPathTests, PlusRemainsAsPlus) {
  // '+' in paths should remain as '+', not be decoded to space
  Uri uri("/path/file+name.txt");
  EXPECT_EQ(uri.getDecodedPath(), "/path/file+name.txt");
}

TEST(UrlDecodedPathTests, EncodedPlus) {
  // Percent-encoded '+' should decode to '+'
  Uri uri("/path/file%2Bname.txt");
  EXPECT_EQ(uri.getDecodedPath(), "/path/file+name.txt");
}

// ==================== COPY AND ASSIGNMENT TESTS ====================

TEST(UriCopyTests, CopyConstructor) {
  Uri uri1("http://example.com:8080/path?q=1#f");
  Uri uri2(uri1);
  EXPECT_TRUE(uri2.isValid());
  EXPECT_EQ(uri2.getScheme(), uri1.getScheme());
  EXPECT_EQ(uri2.getHost(), uri1.getHost());
  EXPECT_EQ(uri2.getPort(), uri1.getPort());
  EXPECT_EQ(uri2.getPath(), uri1.getPath());
  EXPECT_EQ(uri2.getQuery(), uri1.getQuery());
  EXPECT_EQ(uri2.getFragment(), uri1.getFragment());
}

TEST(UriCopyTests, AssignmentOperator) {
  Uri uri1("http://example.com:8080/path?q=1#f");
  Uri uri2;
  // Verify default-constructed state before assignment
  EXPECT_FALSE(uri2.isValid());
  EXPECT_EQ(uri2.getScheme(), "");
  EXPECT_EQ(uri2.getHost(), "");
  EXPECT_EQ(uri2.getPort(), -1);
  EXPECT_EQ(uri2.getPath(), "");
  EXPECT_EQ(uri2.getQuery(), "");
  EXPECT_EQ(uri2.getFragment(), "");

  uri2 = uri1;
  EXPECT_TRUE(uri2.isValid());
  EXPECT_EQ(uri2.getScheme(), uri1.getScheme());
  EXPECT_EQ(uri2.getHost(), uri1.getHost());
  EXPECT_EQ(uri2.getPort(), uri1.getPort());
  EXPECT_EQ(uri2.getPath(), uri1.getPath());
  EXPECT_EQ(uri2.getQuery(), uri1.getQuery());
  EXPECT_EQ(uri2.getFragment(), uri1.getFragment());
}
