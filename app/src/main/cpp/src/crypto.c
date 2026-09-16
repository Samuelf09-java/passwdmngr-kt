#include "../include/crypto.h"
#include "../include/storage.h"
#include "../include/util.h"
#include <openssl/evp.h>
#include <sodium.h>
#include <string.h>
#include <stdlib.h>

bool verify_account(const char *uname, const char *passwd) {

    uint8_t *uname_hash = sha_256_hash((uint8_t *)uname, strlen(uname));

    if (!uname_hash) {
        util_log(LOG_ERROR, "Failed to hash username");
        return false;
    }

    for (int i = 0; i < num_accounts; i++)
        if (!memcmp(accounts[i].uname_hash, uname_hash, HASH_LEN)) {
            free(uname_hash);
            return verify_pwhash(accounts[i].passwd_hash, (char *)passwd);
        }

    // invalid uname
    free(uname_hash);
    return false;
}

bool verify_pwhash(uint8_t *hash, char *passwd) {
    uint8_t *new_hash = ec_malloc(HASH_LEN + SALT_LEN);
    if (!hash_pw_with_salt(passwd, new_hash, HASH_LEN + SALT_LEN, hash)) {
        util_log(LOG_ERROR, "Failed to hash provided password");
        return false;
    }

    return !memcmp(hash, new_hash, HASH_LEN + SALT_LEN);
}

bool hash_pw_with_salt(const char *passwd, uint8_t *out, size_t out_len, uint8_t *salt) {

    if (out_len != HASH_LEN + SALT_LEN) {
        util_log(LOG_ERROR, "Output buffer too small for password hash + salt");
        return false;
    }

    if (crypto_pwhash(out + 16, HASH_LEN, passwd, strlen(passwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT)) {
        util_log(LOG_ERROR, "Failed to hash password with crypto_pwhash");
        return false;
    }

    memcpy(out, salt, SALT_LEN);
    return true;
}

// generates new salt
bool hash_pw(const char *passwd, uint8_t *out, size_t out_len) {

    uint8_t salt[SALT_LEN];
    randombytes_buf(salt, SALT_LEN);

    return hash_pw_with_salt(passwd, out, out_len, salt);
}

char *hash_uname(const char *uname) {
    uint8_t *hash_bin = sha_256_hash((uint8_t *)uname, strlen(uname));

    char *uname_hash = ec_malloc(65);
    if (!uname_hash)
        return NULL;
    for (int i = 0; i < 32; i++)
        sprintf(&uname_hash[i * 2], "%02x", hash_bin[i]);

    uname_hash[64] = '\0';
    return uname_hash;
}

uint8_t *sha_256_hash(uint8_t *data, size_t len) {
    uint8_t *hash_buf = ec_malloc(32);
    uint32_t out_len;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    if (!ctx)
        return NULL;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 || EVP_DigestUpdate(ctx, data, len) != 1 ||
        EVP_DigestFinal_ex(ctx, hash_buf, &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }

    EVP_MD_CTX_free(ctx);

    return hash_buf;
}

bool derive_vault_key(const char *passwd, const uint8_t *salt, uint8_t *key_out, size_t key_len) {
    if (key_len != 32)
        return false;

    /*
     * Because both the password hash in accounts.bin and the encryption key for the vault
     * are generated in the same way, we add an extra char to the password used here to ensure
     * the vault key can never be accidentally stored in accounts.bin due to a salt collision,
     * however unlikely this may be.
     */

    char *enc_passwd = ec_malloc(strlen(passwd) + 2);
    strcpy(enc_passwd, passwd);
    enc_passwd[strlen(passwd)]     = 'e';
    enc_passwd[strlen(passwd) + 1] = 0;

    bool res =
        !crypto_pwhash(key_out, key_len, enc_passwd, strlen(enc_passwd), salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                       crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_DEFAULT);
    wipe_mem(enc_passwd, strlen(enc_passwd));
    return res;
}

char *gen_passwd(int len, char *special, bool digits, bool uppers, bool lowers) {

    if (len <= 0) {
        util_log(LOG_ERROR, "Invalid password length in gen_passwd!");
        return NULL;
    }

    int spec_len  = strlen(special);
    int num_chars = spec_len;
    if (digits)
        num_chars += 10;
    if (uppers)
        num_chars += 26;
    if (lowers)
        num_chars += 26;
    if (!num_chars) {
        util_log(LOG_ERROR, "No characters to generate password with!");
        return NULL;
    }

    char *out = ec_malloc(len + 1);

    for (int i = 0; i < len; i++) {
        int rand_num = randombytes_uniform(num_chars);

        if (rand_num < spec_len) {
            out[i] = special[rand_num];
            continue;
        }

        rand_num -= spec_len;

        if (digits) {
            if (rand_num < 10) {
                out[i] = 0x30 + rand_num;
                continue;
            } else
                rand_num -= 10;
        }

        if (uppers) {
            if (rand_num < 26) {
                out[i] = 0x41 + rand_num;
                continue;
            } else
                rand_num -= 26;
        }

        if (lowers) {
            if (rand_num < 26) {
                out[i] = 0x61 + rand_num;
                continue;
            } else {
                util_log(LOG_ERROR, "rand_num is greater than num_chars! (bug)");
                free(out);
                return NULL;
            }
        }
    }

    out[len] = 0;
    return out;
}

int aes_gcm_encrypt(uint8_t *plaintext, int plaintext_len, uint8_t *key, uint8_t *iv, int iv_len, uint8_t *ciphertext,
                    uint8_t *tag) {
    X(iv_len);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int             len, ciphertext_len;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);

    EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
    ciphertext_len = len;

    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

int aes_gcm_decrypt(uint8_t *ciphertext, int ciphertext_len, uint8_t *key, uint8_t *iv, int iv_len, uint8_t *tag,
                    uint8_t *plaintext) {
    X(iv_len);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int             len, plaintext_len;

    if (!ctx)
        return -1;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}