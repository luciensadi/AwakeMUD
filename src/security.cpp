#include <time.h>
#include <string.h>
#include <sodium.h>

#include "utils.h"
#include "structs.h"
#include "comm.h"
#include "newdb.h"
#include "security.h"

#ifdef NOCRYPT
bool run_crypto_tests() {return TRUE;}
void hash_and_store_password(const char* password, char* array_to_write_to) {
  strcpy(array_to_write_to, password);
}
bool validate_password(const char* password, const char* hashed_password) {
  return strncmp(password, hashed_password) != NULL;
}
bool validate_and_update_password(const char* password, char* hashed_password) {
  return strncmp(password, hashed_password) != NULL;
}
#else


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
  
  // Output for entertainment.
  log(hashed_password);
  
  // Check if our test password passes verification (is it reproducible).
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: We failed to validate the initial argon2 test password after hashing.");
    exit(1);
  }
  
  gettimeofday(&end, (struct timezone *) 0);
  long elapsed = ((end.tv_sec - start.tv_sec) * 1000000L + end.tv_usec) - start.tv_usec;
  long elapsed_secs = (int) (elapsed / 1000000L);
  sprintf(message_buf, "Reproducibility tests completed in %ld microseconds (approx ~%ld sec).",
          elapsed, elapsed_secs);
  log(message_buf);
  
  if (elapsed > (long) (ACCEPTABLE_HASH_TIME_IN_SECONDS * 1000000L)) {
    log("FATAL ERROR: Hashing time exceeded the allowable threshold. Modify security.h to increase your threshold, or lower your OPSLIMIT and MEMLIMIT values. WARNING: CHANGING OPSLIMIT OR MEMLIMIT WILL BREAK ALL YOUR OLD PASSWORDS.");
    exit(1);
  }
  
  // Begin test of the conversion algorithms.
  strncpy(hashed_password, CRYPT(INSECURE_TEST_PASSWORD, "TestChar"), MAX_PWD_LENGTH);
  
  // Validate crypt() vs password.
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_password() failed to match a crypt() password with its origin string.");
    exit(1);
  }
  
  // Check crypt() and update to argon2.
  if (!validate_and_update_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_and_update_password() failed to validate a crypt() password.");
    exit(1);
  }
  
  // Confirm update succeeded.
  if (strstr(hashed_password, ARGON2_HASH_PREFIX) == NULL) {
    log("FATAL ERROR: validate_and_update_password() failed to change crypt() password to argon2.");
    exit(1);
  }
  
  // Validate argon2 vs password.
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_password() failed to match an argon2() password with its origin string.");
    exit(1);
  }
  
  // Validate argon2 vs password via update function.
  if (!validate_and_update_password(INSECURE_TEST_PASSWORD, hashed_password)) {
    log("FATAL ERROR: validate_and_update_password() failed to validate argon2() password.");
    exit(1);
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
        exit(1);
      }
}

// validate_password(): Check if a password matches the given hash.
// Returns TRUE on match, FALSE otherwise.
bool validate_password(const char* password, const char* hashed_password) {
  // If this is a new-style argon2, verify with the correct algorithm.
  if (strstr(hashed_password, ARGON2_HASH_PREFIX) != NULL)
    return crypto_pwhash_str_verify(hashed_password, password, strlen(password)) == 0;
  
  // Otherwise, we're dealing with an old crypt() password.
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
  if (strstr(hashed_password, ARGON2_HASH_PREFIX) == NULL) {
    hash_and_store_password(password, hashed_password);
  }
  
  return TRUE;
}
#endif // #ifdef NOCRYPT's #else
