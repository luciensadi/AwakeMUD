#include "../src/utils.h"

#include <catch.hpp>

#include <cstring>

#define REQUIRE_CSTR_EQ(arg1, arg2) \
    REQUIRE( (!strcmp((arg1), (arg2))) )

TEST_CASE("strlcat", "[strlcat]" ) {
    char buf[8];
    const size_t bufsz = sizeof(buf);

    size_t res;

    // Simple case, no truncation
    strcpy(buf, "abc");
    REQUIRE_CSTR_EQ("abc", buf);
    res = strlcat(buf, "def", bufsz);
    REQUIRE_CSTR_EQ("abcdef", buf);
    REQUIRE(6 == res);
    REQUIRE(6 == strlen(buf));

    // Truncate 1 char
    strcpy(buf, "abcd");
    REQUIRE_CSTR_EQ("abcd", buf);
    res = strlcat(buf, "efgh", bufsz);
    REQUIRE_CSTR_EQ("abcdefg", buf);
    REQUIRE(8 == res);
    REQUIRE(7 == strlen(buf));

    // Truncate multiple chars
    strcpy(buf, "abcd");
    REQUIRE_CSTR_EQ("abcd", buf);
    res = strlcat(buf, "efghijklmnop", bufsz);
    REQUIRE_CSTR_EQ("abcdefg", buf);
    REQUIRE(16 == res);
    REQUIRE(7 == strlen(buf));

    // empty src
    strcpy(buf, "abcd");
    REQUIRE_CSTR_EQ("abcd", buf);
    res = strlcat(buf, "", bufsz);
    REQUIRE_CSTR_EQ("abcd", buf);
    REQUIRE(4 == res);
    REQUIRE(4 == strlen(buf));
}

TEST_CASE( "strlcpy", "[strlcpy]") {
    char buf[8];
    const size_t bufsz = sizeof(buf);

    size_t res;

    // Simple case, copy less than buffer size
    res = strlcpy(buf, "abcd", bufsz);
    REQUIRE_CSTR_EQ("abcd", buf);
    REQUIRE(4 == res);
    REQUIRE(4 == strlen(buf));

    // src is 1 char too big (src_len == bufsz)
    res = strlcpy(buf, "abcdefgh", bufsz);
    REQUIRE_CSTR_EQ("abcdefg", buf);
    REQUIRE(8 == res);
    REQUIRE(7 == strlen(buf));

    // src is many chars too big (src_len > bufsz)
    res = strlcpy(buf, "abcdefghijklmnop", bufsz);
    REQUIRE_CSTR_EQ("abcdefg", buf);
    REQUIRE(16 == res);
    REQUIRE(7 == strlen(buf));

    // copy empty string
    res = strlcpy(buf, "", bufsz);
    REQUIRE_CSTR_EQ("", buf);
    REQUIRE(0 == res);
    REQUIRE(0 == strlen(buf));
}