#include <time.h>
#include <string.h>
#include <sodium.h>

#include "utils.h"
#include "structs.h"
#include "comm.h"
#include "security.h"


// Insecure test password is used for crypto tests.
#define INSECURE_TEST_PASSWORD "8h4engowo)(WTWerhio*"
bool run_crypto_tests() {
  char hashed_password[crypto_pwhash_STRBYTES];
  char message_buf[2048];
  struct timeval start, end;
  
  log("Testing hashing and verifying reproducibility of output.");
  gettimeofday(&start, (struct timezone *) 0);
  
  // Hash our test password.
  hash_password(INSECURE_TEST_PASSWORD, hashed_password);
  
  // Output for entertainment.
  log(hashed_password);
  
  // Check if our test password passes verification (is it reproducible).
  if (!validate_password(INSECURE_TEST_PASSWORD, hashed_password))
    return FALSE;
  
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
  
  return TRUE;
}
#undef INSECURE_TEST_PASSWORD

// hash_password(): Hashing is dead simple now.
void hash_password(const char* password, char* array_to_write_to) {
  if (crypto_pwhash_str
      (array_to_write_to, password, strlen(password), CRYPTO_OPSLIMIT, CRYPTO_MEMLIMIT) != 0) {
        /* out of memory */
        log("ERROR: We ran out of memory while hashing a password. Full stop.");
        exit(1);
      }
}

// validate_password(): Check if a password matches the given hash.
bool validate_password(const char* password, const char* hashed_password) {
  return crypto_pwhash_str_verify(hashed_password, password, strlen(password)) == 0;
}
