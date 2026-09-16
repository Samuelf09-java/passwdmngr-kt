#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * New storage structure:
 * appdir
 * |-vaults
 * | |-<uname_hash>.pwmngr (vault)
 * |-accounts.bin
 * |-preferences.json (contains array of preferences, default = no entry)
 * |-passwdmngr.log
 *
 * Vault structure (used for vault + export/backup files; <name> (<bytes>,<type>)):
 * [magic bytes (6,char) | version (4,uint32_t) | sha256 over remaining header + data (32)]
 * [last modified timestamp (8,uint64_t) | num_entries (4,uint32_t) | ciphertext_len (4,uint32_t)]
 * [salt (16) | nonce (12) | tag (16)]
 * [ciphertext (ciphertext_len)]
 */

#define VAULT_MAGIC "PWMNGR"
#define VAULT_SCHEMA_VERSION 2
#define ACCOUNTS_MAGIC "PWMACC"
#define ACCOUNTS_SCHEMA_VERSION 2
#define HASH_LEN 32
#define SALT_LEN 16
#define NONCE_LEN 12
#define TAG_LEN 16

#define MAX_UNAME_LEN 24
#define MAX_PASSWD_LEN 36

typedef struct __attribute__((packed)) AccountHeader {
    char     magic[6];
    uint32_t version;
    uint8_t  hash[HASH_LEN];
    uint32_t num_accounts;
} AccountHeader;

typedef struct __attribute__((packed)) Account {
    uint8_t uname_hash[HASH_LEN];
    uint8_t passwd_hash[HASH_LEN + SALT_LEN];
} Account;

// contains settings for preferences.json
typedef struct UserPref {
    char *uname_hash;
} UserPref;

typedef struct PasswdEntry {
    int   id;
    char *service;
    char *username;
    char *password;
    char *notes;
} PasswdEntry;

typedef struct __attribute__((packed)) VaultHeader {
    char     magic[6];
    uint32_t version;
    uint8_t  hash[HASH_LEN];
    uint64_t timestamp;
    uint32_t num_entries;
    uint32_t ciphertext_len;
    uint8_t  salt[SALT_LEN];
    uint8_t  nonce[NONCE_LEN];
    uint8_t  tag[TAG_LEN];
} VaultHeader;

#define HEADER_LEN sizeof(VaultHeader)

typedef enum _ImportMode { REPLACE, OVERWRITE, PROMPT } ImportMode;

extern int          num_accounts;
extern Account     *accounts;
extern PasswdEntry *entries;

extern char   *tmp_passwd;
extern uint8_t aes_key[32];

// whether key has been loaded yet; used to verify key is valid before attempting to use it
extern bool key_set;

extern char *username;
extern int   num_entries;

extern UserPref *curr_prefs;

bool load_accounts();
bool init_accounts();
void init_pref_json(char *pref_path);
void save_accounts();

int       storage_read_prefs(UserPref **prefs);
bool      storage_save_prefs(UserPref *prefs, int num_prefs);
UserPref *get_user_prefs(char *uname);

bool create_new_account(char *uname, char *passwd);
bool storage_delete_account(char *uname);
bool storage_change_passwd(char *uname, char *new_pass);

uint8_t *get_user_salt();
char    *storage_get_user_vault_path(char *uname);

int  storage_dump_json(char *vault, char **out, uint8_t *key, bool pretty);
bool encrypt_entries(PasswdEntry *entries, int num_entries, uint8_t *salt, uint8_t **ciphertext, int *ciphertext_len,
                     uint8_t **nonce, uint8_t **tag);
bool decrypt_entries_with_key(uint8_t *key, uint8_t *ciphertext, int ciphertext_len, uint8_t *nonce, uint8_t *tag,
                              PasswdEntry **entries, int *num_entries);
bool decrypt_entries(uint8_t *salt, uint8_t *ciphertext, int ciphertext_len, uint8_t *nonce, uint8_t *tag,
                     PasswdEntry **entries, int *num_entries);
int  storage_read_vault_with_key(char *vault_path, uint8_t *key, PasswdEntry **entries, VaultHeader **hdr);
int  storage_read_vault(char *vault_path, PasswdEntry **entries, VaultHeader **hdr);
bool storage_read_user_vault();
bool storage_write_vault(char *vault_path, PasswdEntry *entries, int num_entries, uint8_t *salt);
bool storage_write_user_vault();

int          storage_get_next_id();
PasswdEntry *storage_get_entry(int id);

bool add_entry(PasswdEntry *entry);
bool delete_entry(int id);
bool update_entry(int id, PasswdEntry *new_entry);

void wipe_passwd_entries(PasswdEntry *entries, int num_entries);