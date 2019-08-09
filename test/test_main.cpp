#define CATCH_CONFIG_MAIN
#include <catch.hpp>

void handle_awlua_assert(const char *cond, const char *func, const char *file, int line)
{
   FAIL("awlua assert failed [" << cond << "] " << file << "::" << func << "@" << line);
}