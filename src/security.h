#ifndef _security_h_
#define _security_h_
#ifdef USE_ARGON2_FOR_HASHING

// Change this value to determine how many seconds you're willing to have the MUD hang for each hash.
#define ACCEPTABLE_HASH_TIME_IN_SECONDS 1.5

// VVV  WARNING: IF YOU CHANGE THESE, YOUR OLD PASSWORD HASHES WILL NOT WORK.  VVV
#define CRYPTO_OPSLIMIT crypto_pwhash_OPSLIMIT_MODERATE
#define CRYPTO_MEMLIMIT crypto_pwhash_MEMLIMIT_MODERATE
// ^^^  WARNING: IF YOU CHANGE THESE, YOUR OLD PASSWORD HASHES WILL NOT WORK.  ^^^

bool run_crypto_tests();

void hash_password(const char* password, char* array_to_write_to);
bool validate_password(const char* password, const char* hashed_password);

#endif // #ifndef USE_ARGON2_FOR_HASHING
#endif // #ifndef _security_h_
