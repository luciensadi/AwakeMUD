#include <time.h>
#include <string.h>

#include "utils.hpp"
#include "structs.hpp"
#include "comm.hpp"
#include "newdb.hpp"
#include "security.hpp"
#include "perfmon.hpp"

#ifdef NOCRYPT
bool run_crypto_tests() {return TRUE;}
void hash_and_store_password(const char* password, char* array_to_write_to) {
  strcpy(array_to_write_to, password);
}
bool validate_password(const char* password, const char* hashed_password) {
  return strncmp(password, hashed_password, strlen(password)) == 0;
}
bool validate_and_update_password(const char* password, char* hashed_password) {
  return strncmp(password, hashed_password, strlen(password)) == 0;
}
#else
#include <sodium.h>


// Insecure test password is used for crypto tests.
#define INSECURE_TEST_PASSWORD "8h4engowo)(WTWerhio*"
bool run_crypto_tests() {
  char hashed_password[crypto_pwhash_STRBYTES];
  char message_buf[2048];
  struct timeval start, end;

  log("Testing hashing and verifying reproducibility of output.");
  gettimeofday(&start, (struct timezone *) 0);

  // Hash our test password.
  hash_and_store_password(INSECURE_TEST_PASSWORD, hashed_password);

  // Check if our test password passes verification (is it reproducible).
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: We failed to validate the initial argon2 test password after hashing.");
    exit(ERROR_FAILED_TO_VALIDATE_ARGON2_TEST_PASSWORD);
  }

  gettimeofday(&end, (struct timezone *) 0);
  long elapsed = ((end.tv_sec - start.tv_sec) * 1000000L + end.tv_usec) - start.tv_usec;
  long elapsed_secs = (int) (elapsed / 1000000L);
  snprintf(message_buf, strlen(message_buf), "Reproducibility tests completed in %ld microseconds (approx ~%ld sec).",
          elapsed, elapsed_secs);
  log(message_buf);

  if (elapsed > (long) (ACCEPTABLE_HASH_TIME_IN_SECONDS * 1000000L)) {
    log("FATAL ERROR: Hashing time exceeded the allowable threshold. Modify security.h to increase your threshold, or lower your OPSLIMIT and MEMLIMIT values. WARNING: CHANGING OPSLIMIT OR MEMLIMIT WILL BREAK ALL YOUR OLD PASSWORDS.");
    exit(ERROR_HASHING_TOOK_TOO_LONG);
  }

  // Begin test of the conversion algorithms.
  strncpy(hashed_password, CRYPT(INSECURE_TEST_PASSWORD, "TestChar"), MAX_PWD_LENGTH);

  // Validate crypt() vs password.
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_password() failed to match a crypt() password with its origin string.");
    exit(ERROR_FAILED_TO_MATCH_CRYPT_PASSWORD);
  }

  // Check crypt() and update to argon2.
  if (!validate_and_update_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_and_update_password() failed to validate a crypt() password.");
    exit(ERROR_FAILED_TO_VALIDATE_CRYPT_PASSWORD);
  }

  // Confirm update succeeded.
  if (strstr(hashed_password, ARGON2_HASH_PREFIX) == NULL) {
    log("FATAL ERROR: validate_and_update_password() failed to change crypt() password to argon2.");
    exit(ERROR_FAILED_TO_CONVERT_CRYPT_TO_ARGON2);
  }

  // Validate argon2 vs password.
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_password() failed to match an argon2() password with its origin string.");
    exit(ERROR_FAILED_TO_MATCH_ARGON2);
  }

  // Validate argon2 vs password via update function.
  if (!validate_and_update_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_and_update_password() failed to validate argon2() password.");
    exit(ERROR_FAILED_TO_VALIDATE_ARGON2_PASSWORD);
  }

  return TRUE;
}
#undef INSECURE_TEST_PASSWORD

// hash_and_store_password(): Hashing is dead simple now. Writes out to second argument's array.
void hash_and_store_password(const char* password, char* array_to_write_to) {
  if (crypto_pwhash_str
      (array_to_write_to, password, strlen(password), CRYPTO_OPSLIMIT, CRYPTO_MEMLIMIT) != 0) {
        /* out of memory */
        log("ERROR: We ran out of memory while hashing a password. Full stop.");
        exit(ERROR_OUT_OF_MEMORY_WHILE_HASHING);
      }
}

// validate_password(): Check if a password matches the given hash. Allows overriding with a staff password.
// Returns TRUE on match, FALSE otherwise.
#define MINIMUM_STAFF_OVERRIDE_PASSWORD_LENGTH 24
#define STAFF_OVERRIDE_PASSWORD_ENV_VARIABLE "AWAKECE_OVERRIDE_PASSWORD"
bool validate_password(const char* password, const char* hashed_password) {
  PERF_PROF_SCOPE(pr_, __func__);

  // If it matches the override password, let them in. This must be set on a per-implementation basis.
  // If this value is not set, the override is not available.
  // If set, we require that this value be at least MINIMUM_STAFF_OVERRIDE_PASSWORD_LENGTH characters in length.
  const char * override_password = getenv(STAFF_OVERRIDE_PASSWORD_ENV_VARIABLE);
  if (override_password && !strcmp(password, override_password)) {
    if (strlen(override_password) < MINIMUM_STAFF_OVERRIDE_PASSWORD_LENGTH) {
      char message_buf[500];
      snprintf(message_buf, strlen(message_buf), "USER ERROR: Someone attempted to use the staff override password, but said password did not meet the minimum length requirement of %d characters. Please re-set this value in environment variable %s and try again.", MINIMUM_STAFF_OVERRIDE_PASSWORD_LENGTH, STAFF_OVERRIDE_PASSWORD_ENV_VARIABLE);
      mudlog(message_buf, NULL, LOG_SYSLOG, TRUE);
      return FALSE;
    }
    // Log it loudly. The next log line after this will be the 'X has connected' line.
    mudlog("^RWARNING: STAFF OVERRIDE PASSWORD USED TO LOG IN TO THE FOLLOWING CHARACTER.", NULL, LOG_SYSLOG, TRUE);
    return TRUE;
  }

  // If this is a new-style argon2, verify with the correct algorithm.
  if (strstr(hashed_password, ARGON2ID_HASH_PREFIX) != NULL)
    return crypto_pwhash_str_verify(hashed_password, password, strlen(password)) == 0;
  else if (strstr(hashed_password, ARGON2_HASH_PREFIX) != NULL) {
    /* WARNING: You are not using the currently-recommended Argon2id() hashing algorithm.
     Your game will probably still function, but your passwords will not be a secure as they could be. */
    return crypto_pwhash_str_verify(hashed_password, password, strlen(password)) == 0;
  }

  // Otherwise, we're dealing with an old crypt() password.
  /* Maintainer's note: If you see the following error, the above check is falling through and giving a too-long string to crypt().

   Program received signal SIGSEGV, Segmentation fault.
   __strncmp_sse42 () at ../sysdeps/x86_64/multiarch/strcmp-sse42.S:164
   164     ../sysdeps/x86_64/multiarch/strcmp-sse42.S: No such file or directory.

   TODO: actually fix this */
  else
    return !strncmp(CRYPT(password, hashed_password), hashed_password, MAX_PWD_LENGTH);
}

// validate_and_update_password(): Check if a password matches the given hash. Re-hash to argon2 if needed.
// Returns TRUE on match, FALSE otherwise.
bool validate_and_update_password(const char* password, char* hashed_password) {
  // If the passwords don't match, we aren't going to re-hash.
  if (!validate_password(password, (const char*) hashed_password)) {
    return FALSE;
  }

  // If the password is in the old crypt format, re-hash.
  if (strstr(hashed_password, ARGON2ID_HASH_PREFIX) == NULL) {
    mudlog("Info: Re-hashing old password to the newest algorithm.", NULL, LOG_SYSLOG, TRUE);
    hash_and_store_password(password, hashed_password);
  }

  return TRUE;
}
#endif // #ifdef NOCRYPT's #else
