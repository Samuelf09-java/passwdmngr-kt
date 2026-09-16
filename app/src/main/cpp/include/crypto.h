#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool verify_account(const char *uname, const char *passwd);

bool  verify_pwhash(uint8_t *hash, char *passwd);
bool  hash_pw_with_salt(const char *passwd, uint8_t *out, size_t out_len, uint8_t *salt);
bool  hash_pw(const char *passwd, uint8_t *out, size_t out_len);
char *hash_uname(const char *uname);

uint8_t *sha_256_hash(uint8_t *data, size_t len);

bool  derive_vault_key(const char *passwd, const uint8_t *salt, uint8_t *key_out, size_t key_len);
char *gen_passwd(int len, char *special, bool digits, bool uppers, bool lowers);

int aes_gcm_encrypt(uint8_t *plaintext, int plaintext_len, uint8_t *key, uint8_t *iv, int iv_len, uint8_t *ciphertext,
                    uint8_t *tag);

int aes_gcm_decrypt(uint8_t *ciphertext, int ciphertext_len, uint8_t *key, uint8_t *iv, int iv_len, uint8_t *tag,
                    uint8_t *plaintext);