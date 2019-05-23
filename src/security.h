#ifndef _security_h_
#define _security_h_

// Change this value to determine how many seconds you're willing to have the MUD hang for each hash.
// TODO: Was 1.5
#define ACCEPTABLE_HASH_TIME_IN_SECONDS 100

// Warning: Changing these will not affect the hashing speed of old password hashes. It may also break old hashes, this has not been tested.
#define CRYPTO_OPSLIMIT crypto_pwhash_OPSLIMIT_INTERACTIVE
#define CRYPTO_MEMLIMIT crypto_pwhash_MEMLIMIT_INTERACTIVE

// The prefix string used to identify whether a password is argon2 or crypt().
#define ARGON2ID_HASH_PREFIX "$argon2id$"
#define ARGON2_HASH_PREFIX "$argon2"

bool run_crypto_tests();

void hash_and_store_password(const char* password, char* array_to_write_to);
bool validate_password(const char* password, const char* hashed_password);
bool validate_and_update_password(const char* password, char* hashed_password);

#endif // #ifndef _security_h_
