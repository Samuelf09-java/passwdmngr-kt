#include "storage.h"
#include "crypto.h"
#include "util.h"
#include "cJSON.h"
#include "sodium.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int          num_accounts;
Account     *accounts = NULL;
PasswdEntry *entries  = NULL;

char   *tmp_passwd = NULL;
uint8_t aes_key[32];
bool    key_set = false;

char *username    = NULL;
int   num_entries = 0;

UserPref *curr_prefs = NULL;

static int compare_entries_by_service(const void *a, const void *b) { // Sort alphabetically by service
    const PasswdEntry *ea = a;
    const PasswdEntry *eb = b;
    return strcasecmp(ea->service, eb->service);
}

bool load_accounts() {

    char *accounts_path = util_get_accounts_file();
    FILE *fp            = fopen(accounts_path, "rb");

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize < (int64_t)sizeof(AccountHeader)) {
        util_log(LOG_ERROR, "accounts.bin is too small to contain a valid header!");
        fclose(fp);
        return false;
    }

    uint8_t *accounts_buf = ec_malloc(fsize);
    ec_fread(accounts_buf, 1, fsize, fp);
    fclose(fp);

    AccountHeader *hdr = (AccountHeader *)accounts_buf;

    int64_t expected_size = sizeof(AccountHeader) + hdr->num_accounts * sizeof(Account);

    if (fsize < expected_size) {
        util_log(LOG_ERROR, "accounts.bin truncated or corrupted");
        free(accounts_buf);
        return false;
    }

    if (memcmp(hdr->magic, ACCOUNTS_MAGIC, 6)) { // invalid magic
        util_log(LOG_FATAL, "Invalid file format! (wrong magic bytes)");
        free(accounts_buf);
        return false;
    }

    if (hdr->version != ACCOUNTS_SCHEMA_VERSION) {
        util_log(LOG_FATAL, "Invalid file format! (wrong version)");
        free(accounts_buf);
        return false;
    }

    uint8_t *hash = sha_256_hash(accounts_buf + 10 + HASH_LEN, fsize - 10 - HASH_LEN);
    if (!util_check_ptr(hash, "Failed to hash accounts data!")) {
        free(accounts_buf);
        return false;
    }

    if (memcmp(hdr->hash, hash, HASH_LEN)) {
        util_log(LOG_FATAL, "Could not verify accounts.bin integrity; hashes do not match!");
        // free(hash);
        // free(accounts_buf);
        // return false;
    }

    free(hash);

    num_accounts = hdr->num_accounts;
    accounts     = ec_malloc(sizeof(Account) * num_accounts);
    int i        = 0;

    for (uint8_t *p = accounts_buf + sizeof(AccountHeader);
         p < accounts_buf + sizeof(AccountHeader) + num_accounts * sizeof(Account); p += sizeof(Account))
        memcpy(&accounts[i++], p, sizeof(Account));

    free(accounts_buf);

    return true;
}

bool init_accounts() {

    AccountHeader *hdr = ec_malloc(sizeof(AccountHeader));
    memcpy(hdr->magic, ACCOUNTS_MAGIC, 6);
    hdr->version      = ACCOUNTS_SCHEMA_VERSION;
    hdr->num_accounts = 0;

    uint8_t *hash =
        sha_256_hash((uint8_t *)((uint8_t *)hdr + sizeof(AccountHeader) - 10 - HASH_LEN), sizeof(AccountHeader) - 10 - HASH_LEN);
    if (!util_check_ptr(hash, "Failed to hash accounts header")) {
        free(hdr);
        return false;
    }

    memcpy(hdr->hash, hash, HASH_LEN);

    char *accounts_path = util_get_accounts_file();

    FILE *fp = fopen(accounts_path, "wb");
    if (!util_check_ptr(fp, "Failed to open accounts file")) {
        free(hdr);
        free(accounts_path);
        return false;
    }

    fwrite(hdr, 1, sizeof(AccountHeader), fp);
    fclose(fp);
    return true;
}

void init_pref_json(char *pref_path) {

    cJSON *root = cJSON_CreateObject();
    cJSON_AddArrayToObject(root, "preferences");

    char *pref_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    FILE *fp = fopen(pref_path, "w");
    if (!fp) {
        util_log(LOG_ERROR, "Failed to open/create preferences file at %s!", pref_path);
        return;
    }
    fwrite(pref_json, 1, strlen(pref_json), fp);
    fclose(fp);
}

void save_accounts() {

    uint8_t       *write_buf = ec_malloc(sizeof(AccountHeader) + sizeof(Account) * num_accounts);
    AccountHeader *hdr       = (AccountHeader *)write_buf;

    memcpy(hdr->magic, ACCOUNTS_MAGIC, 6);
    hdr->version      = ACCOUNTS_SCHEMA_VERSION;
    hdr->num_accounts = num_accounts;

    int i = 0;
    for (uint8_t *p = write_buf + sizeof(AccountHeader);
         p < write_buf + sizeof(AccountHeader) + sizeof(Account) * num_accounts; p += sizeof(Account))
        memcpy(p, &accounts[i++], sizeof(Account));

    uint8_t *hash =
        sha_256_hash(write_buf + 10 + HASH_LEN, sizeof(AccountHeader) - 10 - HASH_LEN + sizeof(Account) * num_accounts);
    if (!util_check_ptr(hash, "Failed to hash account data for writing to accounts.bin")) {
        free(write_buf);
        return;
    }

    memcpy(hdr->hash, hash, HASH_LEN);
    free(hash);

    char *accounts_path = util_get_accounts_file();
    FILE *fp            = fopen(accounts_path, "wb");
    free(accounts_path);
    if (!util_check_ptr(fp, "Failed to open accounts file for writing")) {
        free(write_buf);
        return;
    }

    fwrite(write_buf, 1, sizeof(AccountHeader) + sizeof(Account) * num_accounts, fp);
    fclose(fp);
}

int storage_read_prefs(UserPref **prefs) {

    char *prefs_path = util_get_prefs_file();
    if (!util_check_ptr(prefs_path, "Failed to build path to preferences.json"))
        return -1;

    FILE *fp = fopen(prefs_path, "r");
    if (!fp) {
        util_log(LOG_ERROR, "Failed to read preferences file!");
        return -4;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *prefs_json = ec_malloc(fsize);
    ec_fread(prefs_json, 1, fsize, fp);
    fclose(fp);

    cJSON *root = NULL;

    if (!(root = cJSON_Parse(prefs_json))) {
        util_log(LOG_ERROR, "Failed to parse preferences.json");
        free(prefs_json);
        return -2;
    }

    free(prefs_json);

    cJSON *prefs_array = cJSON_GetObjectItemCaseSensitive(root, "preferences");
    if (!util_check_ptr(prefs_array, "preferences.json missing 'preferences' array")) {
        cJSON_Delete(root);
        return -3;
    }

    int num_prefs = cJSON_GetArraySize(prefs_array);
    *prefs        = ec_malloc(sizeof(UserPref) * num_prefs);

    for (int i = 0; i < num_prefs; i++) {
        cJSON *entry = cJSON_GetArrayItem(prefs_array, i);

        const char *uname_hash = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(prefs_array, "uname_hash"));

        (*prefs)[i].uname_hash = strdup(uname_hash);
    }

    cJSON_Delete(root);
    return num_prefs;
}

bool storage_save_prefs(UserPref *prefs, int num_prefs) {
    cJSON *root = cJSON_CreateObject();
    cJSON *prefs_array = cJSON_AddArrayToObject(root, "preferences");

    for (int i = 0; i < num_prefs; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "uname_hash", prefs[i].uname_hash);
        cJSON_AddItemToArray(prefs_array, obj);
    }

    char *prefs_json = cJSON_Print(root);
    cJSON_Delete(root);

    char *prefs_path = util_get_prefs_file();
    if (!util_check_ptr(prefs_path, "Failed to build path to preferences.json")) {
        free(prefs_json);
        return false;
    }

    FILE *fp = fopen(prefs_path, "w");
    if (!fp) {
        util_log(LOG_ERROR, "Failed to open preferences file at %s!", prefs_path);
        free(prefs_path);
        free(prefs_json);
        return false;
    }
    fwrite(prefs_json, 1, strlen(prefs_json), fp);
    fclose(fp);

    free(prefs_path);
    free(prefs_json);
    return true;
}

UserPref *get_user_prefs(char *uname) {

    char *uname_hash = hash_uname(uname);
    if (!util_check_ptr(uname_hash, "Failed to hash username to get preferences"))
        return NULL;

    UserPref *prefs     = NULL;
    int       num_prefs = storage_read_prefs(&prefs);

    for (int i = 0; i < num_prefs; i++)
        if (!strcmp(prefs[i].uname_hash, uname_hash)) {
            free(uname_hash);
            UserPref *pref = ec_malloc(sizeof(UserPref));
            memcpy(pref, &prefs[i], sizeof(UserPref));
            free(prefs);
            return pref;
        }

    // if we reach here, user has no entry; use defaults

    if (prefs)
        free(prefs);

    UserPref *pref   = ec_malloc(sizeof(UserPref));
    pref->uname_hash = uname_hash;
    // fill in other defaults as needed

    return pref;
}

bool create_new_account(char *uname, char *passwd) {

    uint8_t *new_uname_hash = sha_256_hash((uint8_t *)uname, strlen(uname));
    if (!util_check_ptr(new_uname_hash, "Failed to hash username"))
        return false;

    for (int i = 0; i < num_accounts; i++) {
        if (memcmp(accounts[i].uname_hash, new_uname_hash, HASH_LEN) == 0) {
            free(new_uname_hash);
            util_log(LOG_ERROR, "Duplicate username");
            return false;
        }
    }

    uint8_t *new_passwd_hash = ec_malloc(HASH_LEN + SALT_LEN);

    if (!hash_pw(passwd, new_passwd_hash, HASH_LEN + SALT_LEN)) {
        util_log(LOG_ERROR, "Failed to hash new password");
        free(new_uname_hash);
        free(new_passwd_hash);
        return false;
    }

    num_accounts++;

    accounts = ec_realloc(accounts, sizeof(Account) * num_accounts);
    if (!accounts) {
        num_accounts--;
        free(new_uname_hash);
        free(new_passwd_hash);
        util_log(LOG_ERROR, "Failed to expand accounts array");
        return false;
    }

    memcpy(accounts[num_accounts - 1].uname_hash, new_uname_hash, HASH_LEN);
    memcpy(accounts[num_accounts - 1].passwd_hash, new_passwd_hash, HASH_LEN + SALT_LEN);

    free(new_uname_hash);
    free(new_passwd_hash);

    save_accounts();

    username = strdup(uname);

    char    *vault_path = storage_get_user_vault_path(uname);
    uint8_t *salt       = ec_malloc(SALT_LEN);
    randombytes_buf(salt, SALT_LEN);

    if (!storage_write_vault(vault_path, NULL, 0, salt)) {
        util_log(LOG_ERROR, "Failed to initialize user vault!");
        free(vault_path);
        free(salt);
        return false;
    }

    free(vault_path);
    free(salt);

    util_log(LOG_INFO, "Created new account & vault; saved to accounts.bin");

    return true;
}

static bool delete_user_data(char *uname) {
    char *vault_path = storage_get_user_vault_path(uname);
    if (!util_check_ptr(vault_path, "Failed to get user vault path"))
        return false;

    if (!remove(vault_path)) {
        util_log(LOG_ERROR, "Failed to delete user vault");
        free(vault_path);
        return false;
    }
    free(vault_path);

    char *uname_hash = hash_uname(uname);
    if (!util_check_ptr(uname_hash, "Failed to hash username for preferences lookup"))
        return false;

    UserPref *prefs     = NULL;
    int       num_prefs = storage_read_prefs(&prefs);
    if (num_prefs < 0) {
        util_log(LOG_ERROR, "Failed to load user preferences");
        free(uname_hash);
        return false;
    }

    for (int i = 0; i < num_prefs; i++)
        if (!strcmp(prefs[i].uname_hash, uname_hash)) {
            free(prefs[i].uname_hash);
            free(uname_hash);
            for (int j = i; j < num_prefs - 1; j++)
                prefs[j] = prefs[j + 1];

            prefs = ec_realloc(prefs, --num_prefs * sizeof(UserPref));
            if (!prefs)
                return false;
            storage_save_prefs(prefs, num_prefs);
            return true;
        }

    util_log(LOG_DEBUG, "No preferences found for user %s", uname);
    return true;
}

bool storage_delete_account(char *uname) {

    // delete data
    if (!delete_user_data(uname)) {
        util_log(LOG_ERROR, "Failed to delete user data");
        return false;
    }

    // remove entry from accounts.json
    uint8_t *uname_hash = sha_256_hash((uint8_t *)uname, strlen(uname));
    for (int i = 0; i < num_accounts; i++) {
        if (!memcmp(accounts[i].uname_hash, uname_hash, HASH_LEN)) {
            free(uname_hash);

            for (int j = i; j < num_accounts - 1; j++)
                accounts[j] = accounts[j + 1];

            num_accounts--;
            accounts = ec_realloc(accounts, sizeof(Account) * num_accounts);
            if (!accounts)
                return false;

            save_accounts();
            return true;
        }
    }

    free(uname_hash);
    util_log(LOG_FATAL, "Could not find user account to delete");
    return false;
}

// Expects account has already been verified before calling
bool storage_change_passwd(char *uname, char *new_pass) {

    uint8_t *uname_hash = sha_256_hash((uint8_t *)uname, strlen(uname));
    for (int i = 0; i < num_accounts; i++) {
        if (!memcmp(accounts[i].uname_hash, uname_hash, HASH_LEN)) {
            free(uname_hash);
            uint8_t *new_passwd_hash = ec_malloc(HASH_LEN + SALT_LEN);
            if (!hash_pw(new_pass, new_passwd_hash, HASH_LEN + SALT_LEN)) {
                free(new_passwd_hash);
                util_log(LOG_FATAL, "Failed to hash password");
                return false;
            }

            memcpy(accounts[i].passwd_hash, new_passwd_hash, HASH_LEN + SALT_LEN);
            free(new_passwd_hash);
            save_accounts();

            // force reencryption with new key
            key_set    = false;
            tmp_passwd = strdup(new_pass);
            wipe_mem(aes_key, sizeof(aes_key));

            return storage_write_user_vault();
        }
    }

    free(uname_hash);
    util_log(LOG_FATAL, "Failed to find account with username %s in accounts array", uname);
    return false;
}

uint8_t *get_user_salt() {
    char        *vault_path = storage_get_user_vault_path(username);
    VaultHeader *hdr        = ec_malloc(HEADER_LEN);
    FILE        *fp         = fopen(vault_path, "rb");
    free(vault_path);
    if (!fp) {
        util_log(LOG_ERROR, "Failed to open vault file for reading salt");
        free(hdr);
        return NULL;
    }
    ec_fread(hdr, 1, HEADER_LEN, fp);
    fclose(fp);
    uint8_t *salt = ec_malloc(SALT_LEN);
    memcpy(salt, hdr->salt, SALT_LEN);
    free(hdr);
    return salt;
}

char *storage_get_user_vault_path(char *uname) {

    if (!util_check_ptr(uname, "Failed to build path to user vault: uname is NULL"))
        return NULL;

    char *uname_hash = hash_uname(uname);
    if (!util_check_ptr(uname_hash, "Failed to hash username"))
        return NULL;

    if (!util_check_ptr(app_dir, "Failed to get app dir")) {
        free(uname_hash);
        return NULL;
    }

    int   path_len = strlen(app_dir) + strlen("vaults") + 1 + strlen(uname_hash) + strlen(".pwmngr") + 1;
    char *path     = ec_malloc(path_len);
    sprintf(path, "%svaults%c%s.pwmngr", app_dir, PATH_SEPARATOR, uname_hash);
    free(uname_hash);
    return path;
}

/*
 * yes, this has duplicate logic, but it makes more since for this to be
 * in its own function, since the others return a PasswdEntry array built
 * directly from json, and it would be more complicated to make them work
 * off a function that just returns raw json
 */
int storage_dump_json(char *vault, char **out, uint8_t *key, bool pretty) {
    FILE *fp = fopen(vault, "rb");
    if (!fp) {
        util_log(LOG_ERROR, "Failed to open vault");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= (int64_t)HEADER_LEN) {
        util_log(LOG_ERROR, "Vault is too small to contain any data!");
        return -2;
    }

    uint8_t *vault_buf = ec_malloc(fsize);
    ec_fread(vault_buf, 1, fsize, fp);
    fclose(fp);

    VaultHeader *hdr = ec_malloc(HEADER_LEN);
    memcpy(hdr, vault_buf, HEADER_LEN);

    if (memcmp(hdr->magic, VAULT_MAGIC, 6)) { // invalid magic
        util_log(LOG_FATAL, "Invalid file format! (wrong magic bytes)");
        free(vault_buf);
        return -3;
    }

    if (hdr->version != VAULT_SCHEMA_VERSION) {
        util_log(LOG_FATAL, "Invalid file format! (wrong version)");
        free(vault_buf);
        return -4;
    }

    uint8_t *hash = sha_256_hash(vault_buf + 10 + HASH_LEN, fsize - 10 - HASH_LEN);
    if (!util_check_ptr(hash, "Failed to hash vault data!")) {
        free(vault_buf);
        return -5;
    }

    if (memcmp(hdr->hash, hash, HASH_LEN)) {
        util_log(LOG_FATAL, "Could not verify vault integrity; hashes do not match!");
        free(hash);
        free(vault_buf);
        return -6;
    }

    free(hash);

    uint8_t *ciphertext = vault_buf + HEADER_LEN;

    if (key == NULL) {
        if (!key_set) {
            if (!derive_vault_key(tmp_passwd, hdr->salt, aes_key, sizeof(aes_key))) {
                util_log(LOG_ERROR, "Failed to derive vault key");
                return -9;
            }

            key_set = true;
            wipe_mem(tmp_passwd, strlen(tmp_passwd));
            free(tmp_passwd);
            tmp_passwd = NULL;
        }
        key = aes_key;
    }

    uint8_t *plaintext = ec_malloc(hdr->ciphertext_len + 1);

    int plaintext_len =
        aes_gcm_decrypt(ciphertext, hdr->ciphertext_len, key, hdr->nonce, NONCE_LEN, hdr->tag, plaintext);
    plaintext[plaintext_len] = 0;

    free(vault_buf);
    if (plaintext_len < 0) {
        util_log(LOG_ERROR, "Vault decryption failed (wrong password/corrupted vault)");
        free(plaintext);
        return -7;
    }

    if (pretty) {
        cJSON *root = cJSON_Parse((char *)plaintext);

        if (!root) {
            util_log(LOG_ERROR, "Failed to parse JSON string");
            free(plaintext);
            return -8;
        }

        *out = cJSON_Print(root);

        cJSON_Delete(root);
        free(plaintext);
        return strlen(*out);
    }

    *out = (char *)plaintext;
    return plaintext_len;
}

bool encrypt_entries(PasswdEntry *entries, int num_entries, uint8_t *salt, uint8_t **ciphertext, int *ciphertext_len,
                     uint8_t **nonce, uint8_t **tag) {
    cJSON *root = cJSON_CreateObject();
    cJSON *entries_arr = cJSON_AddArrayToObject(root, "entries");

    for (int i = 0; i < num_entries; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", entries[i].id);
        cJSON_AddStringToObject(obj, "service", entries[i].service);
        cJSON_AddStringToObject(obj, "username", entries[i].username);
        cJSON_AddStringToObject(obj, "password", entries[i].password);
        cJSON_AddStringToObject(obj, "notes", entries[i].notes);
        cJSON_AddItemToArray(entries_arr, obj);
    }

    char *json_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!key_set) {
        if (!derive_vault_key(tmp_passwd, salt, aes_key, sizeof(aes_key))) {
            util_log(LOG_ERROR, "Failed to derive vault key");
            free(json_data);
            return false;
        }
        key_set = true;

        // get plaintext password out of memory
        wipe_mem(tmp_passwd, strlen(tmp_passwd));
        free(tmp_passwd);
        tmp_passwd = NULL;
    }

    *nonce = ec_malloc(12);
    randombytes_buf(*nonce, 12);

    int plaintext_len = strlen(json_data);
    *ciphertext       = ec_malloc(plaintext_len + 16);
    *tag              = ec_malloc(16);

    *ciphertext_len = aes_gcm_encrypt((uint8_t *)json_data, plaintext_len, aes_key, *nonce, 12, *ciphertext, *tag);

    free(json_data);

    if (*ciphertext_len <= 0) {
        util_log(LOG_ERROR, "Vault encryption failed");
        free(*ciphertext);
        free(*nonce);
        free(*tag);
        *ciphertext = NULL;
        *nonce      = NULL;
        *tag        = NULL;
        return false;
    }

    return true;
}

bool decrypt_entries_with_key(uint8_t *key, uint8_t *ciphertext, int ciphertext_len, uint8_t *nonce, uint8_t *tag,
                              PasswdEntry **entries, int *num_entries) {
    uint8_t *plaintext = ec_malloc(ciphertext_len);

    int plaintext_len = aes_gcm_decrypt(ciphertext, ciphertext_len, key, nonce, 12, tag, plaintext);

    if (plaintext_len < 0) {
        util_log(LOG_ERROR, "Vault decryption failed (wrong password/corrupted vault)");
        free(plaintext);
        return false;
    }

    cJSON *root = cJSON_Parse((char *)plaintext);
    if (!root) {
        util_log(LOG_ERROR, "Failed to load json from decrypted vault.bin");
        free(plaintext);
        return false;
    }

    cJSON *entries_array = cJSON_GetObjectItemCaseSensitive(root, "entries");
    if (!entries_array) {
        util_log(LOG_ERROR, "decrypted data missing 'entries' array");
        cJSON_Delete(root);
        return false;
    }

    *num_entries = cJSON_GetArraySize(entries_array);

    *entries = ec_malloc(sizeof(PasswdEntry) * *num_entries);
    if (!entries) {
        util_log(LOG_ERROR, "malloc failed for passwdentry array");
        return false;
    }

    for (int i = 0; i < *num_entries; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries_array, i);

        int         id       = cJSON_GetNumberValue(cJSON_GetObjectItem(entry, "id"));
        const char *service  = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "service"));
        const char *username = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "username"));
        const char *password = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "password"));
        const char *notes    = cJSON_GetStringValue(cJSON_GetObjectItem(entry, "notes"));

        (*entries)[i].id       = id;
        (*entries)[i].service  = strdup(service);
        (*entries)[i].username = strdup(username);
        (*entries)[i].password = strdup(password);
        (*entries)[i].notes    = strdup(notes);
    }

    cJSON_Delete(root);
    free(plaintext);

    return true;
}

// Uses current user's key
bool decrypt_entries(uint8_t *salt, uint8_t *ciphertext, int ciphertext_len, uint8_t *nonce, uint8_t *tag,
                     PasswdEntry **entries, int *num_entries) {
    if (!key_set) {
        if (!derive_vault_key(tmp_passwd, salt, aes_key, sizeof(aes_key))) {
            util_log(LOG_ERROR, "Failed to derive vault key");
            return false;
        }

        key_set = true;

        // get plaintext password out of memory
        wipe_mem(tmp_passwd, strlen(tmp_passwd));
        free(tmp_passwd);
        tmp_passwd = NULL;
    }

    return decrypt_entries_with_key(aes_key, ciphertext, ciphertext_len, nonce, tag, entries, num_entries);
}

int storage_read_vault_with_key(char *vault_path, uint8_t *key, PasswdEntry **entries, VaultHeader **hdr) {
    FILE *fp = fopen(vault_path, "rb");
    if (!fp) {
        util_log(LOG_ERROR, "Failed to open vault");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= (int64_t)HEADER_LEN) {
        util_log(LOG_ERROR, "Vault is too small to contain any data!");
        return -2;
    }

    uint8_t *vault_buf = ec_malloc(fsize);
    ec_fread(vault_buf, 1, fsize, fp);
    fclose(fp);

    *hdr = ec_malloc(HEADER_LEN);
    memcpy(*hdr, vault_buf, HEADER_LEN);

    if (memcmp((*hdr)->magic, VAULT_MAGIC, 6)) { // invalid magic
        util_log(LOG_FATAL, "Invalid file format! (wrong magic bytes)");
        free(vault_buf);
        return -3;
    }

    if ((*hdr)->version != VAULT_SCHEMA_VERSION) {
        util_log(LOG_FATAL, "Invalid file format! (wrong version)");
        free(vault_buf);
        return -4;
    }

    uint8_t *hash = sha_256_hash(vault_buf + 10 + HASH_LEN, fsize - 10 - HASH_LEN);
    if (!util_check_ptr(hash, "Failed to hash vault data!")) {
        free(vault_buf);
        return -5;
    }

    if (memcmp((*hdr)->hash, hash, HASH_LEN)) {
        util_log(LOG_FATAL, "Could not verify vault integrity; hashes do not match!");
        free(hash);
        free(vault_buf);
        return -6;
    }

    free(hash);

    uint8_t *ciphertext  = vault_buf + HEADER_LEN;
    int      num_entries = -8;

    if (key == NULL) {
        if (!key_set) {
            if (!derive_vault_key(tmp_passwd, (*hdr)->salt, aes_key, sizeof(aes_key))) {
                util_log(LOG_ERROR, "Failed to derive vault key");
                return -9;
            }

            key_set = true;
            wipe_mem(tmp_passwd, strlen(tmp_passwd));
            free(tmp_passwd);
            tmp_passwd = NULL;
        }
        key = aes_key;
    }

    if (!decrypt_entries_with_key(key, ciphertext, (*hdr)->ciphertext_len, (*hdr)->nonce, (*hdr)->tag, entries,
                                  &num_entries)) {
        util_log(LOG_FATAL, "Failed to decrypt entries array");
        free(vault_buf);
        return -7;
    }

    free(vault_buf);
    return num_entries;
}

int storage_read_vault(char *vault_path, PasswdEntry **entries, VaultHeader **hdr) {
    // key is autofilled with current user's key if NULL
    return storage_read_vault_with_key(vault_path, NULL, entries, hdr);
}

bool storage_read_user_vault() {

    char *vault_path = storage_get_user_vault_path(username);
    if (!util_check_ptr(vault_path, "Failed to build user vault path"))
        return false;

    VaultHeader *hdr = ec_malloc(HEADER_LEN);
    num_entries      = storage_read_vault(vault_path, &entries, &hdr);
    if (num_entries < 0) {
        util_log(LOG_ERROR, "Failed to load user vault!");
        free(hdr);
        return false;
    }

    time_t     t             = (time_t)hdr->timestamp;
    struct tm *last_modified = localtime(&t);
    char       time_buf[23];
    strftime(time_buf, sizeof(time_buf), "%m-%d-%Y at %H:%M:%S", last_modified);

    util_log(LOG_DEBUG, "Loaded %d entries from user vault; last modified %s", num_entries, time_buf);

    free(hdr);
    return true;
}

bool storage_write_vault(char *vault_path, PasswdEntry *entries, int num_entries, uint8_t *salt) {

    VaultHeader *hdr = ec_malloc(HEADER_LEN);
    memcpy(hdr->magic, VAULT_MAGIC, 6);
    hdr->version = VAULT_SCHEMA_VERSION;
    memcpy(hdr->salt, salt, SALT_LEN);
    hdr->num_entries = num_entries;

    uint8_t *ciphertext = NULL;
    uint8_t *nonce      = NULL;
    uint8_t *tag        = NULL;

    uint32_t clen = 0;
    if (!encrypt_entries(entries, num_entries, salt, &ciphertext, (int32_t *)&clen, &nonce, &tag)) {
        util_log(LOG_ERROR, "Failed to encrypt user vault");
        free(hdr);
        return false;
    }
    hdr->ciphertext_len = clen;

    memcpy(hdr->nonce, nonce, 12);
    memcpy(hdr->tag, tag, 16);
    free(nonce);
    free(tag);

    uint8_t *write_buf = ec_malloc(HEADER_LEN + hdr->ciphertext_len);

    hdr->timestamp = time(NULL);
    memcpy(write_buf, hdr, HEADER_LEN);
    memcpy(write_buf + HEADER_LEN, ciphertext, hdr->ciphertext_len);
    free(ciphertext);

    uint8_t *hash = sha_256_hash(write_buf + 10 + HASH_LEN, HEADER_LEN + hdr->ciphertext_len - 10 - HASH_LEN);
    if (!util_check_ptr(hash, "Failed to hash vault data for writing")) {
        free(write_buf);
        free(hdr);
        return false;
    }

    memcpy(write_buf + 10, hash, HASH_LEN);
    free(hash);

    FILE *fp = fopen(vault_path, "wb");
    if (!fp) {
        util_log(LOG_ERROR, "Failed to open/create vault for writing");
        free(write_buf);
        free(hdr);
        return false;
    }

    fwrite(write_buf, 1, HEADER_LEN + hdr->ciphertext_len, fp);
    fclose(fp);
    free(hdr);

    return true;
}

bool storage_write_user_vault() {

    char *vault_path = storage_get_user_vault_path(username);
    if (!util_check_ptr(vault_path, "Failed to get user vault path"))
        return false;

    uint8_t *salt = get_user_salt();
    if (!util_check_ptr(salt, "Failed to retrieve salt")) {
        free(vault_path);
        return false;
    }

    bool res = storage_write_vault(vault_path, entries, num_entries, salt);

    free(vault_path);
    free(salt);

    return res;
}

int storage_get_next_id() {
    int current_id = 0;
    for (int i = 0; i < num_entries; i++)
        current_id = MAX(current_id, i[entries].id); // fun c tricks with arrays :)

    // run id 'defrag' routine to consolidate ids if they have run excessively high
    if (current_id > num_entries * 3 || (num_entries > 10000 && current_id > num_entries + 200)) {
        for (int i = 0; i < num_entries; i++)
            entries[i].id = i + 1;
        storage_write_user_vault();
        return num_entries + 1;
    }

    return current_id + 1;
}

PasswdEntry *storage_get_entry(int id) {

    for (int i = 0; i < num_entries; i++)
        if (entries[i].id == id)
            return &entries[i];

    util_log(LOG_ERROR, "Failed to fetch password entry from id: invalid id");
    return NULL;
}

bool add_entry(PasswdEntry *entry) {

    for (int i = 0; i < num_entries; i++) {
        if (!strcmp(entries[i].service, entry->service)) {
            util_log(LOG_ERROR, "Duplicate service name");
            return false;
        }

        if (entries[i].id == entry->id) {
            util_log(LOG_ERROR, "Duplicate id");
            return false;
        }
    }

    num_entries++;

    if (entries)
        entries = ec_realloc(entries, sizeof(PasswdEntry) * num_entries);
    else
        entries = ec_malloc(sizeof(PasswdEntry));

    entries[num_entries - 1].id       = entry->id;
    entries[num_entries - 1].service  = strdup(entry->service);
    entries[num_entries - 1].username = strdup(entry->username);
    entries[num_entries - 1].password = strdup(entry->password);
    entries[num_entries - 1].notes    = strdup(entry->notes);

    qsort(entries, num_entries, sizeof(PasswdEntry), compare_entries_by_service);

    if (!storage_write_user_vault()) {
        util_log(LOG_ERROR, "Failed to write expanded entry list; this session will not be saved properly");
        return false;
    }

    return true;
}

bool delete_entry(int id) {

    int index = -1;
    for (int i = 0; i < num_entries; i++) {
        if (entries[i].id == id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        util_log(LOG_ERROR, "Failed to delete entry: id not found");
        return false;
    }

    wipe_mem(entries[index].service, strlen(entries[index].service));
    free(entries[index].service);
    wipe_mem(entries[index].username, strlen(entries[index].username));
    free(entries[index].username);
    wipe_mem(entries[index].password, strlen(entries[index].password));
    free(entries[index].password);
    wipe_mem(entries[index].notes, strlen(entries[index].notes));
    free(entries[index].notes);

    for (int j = index; j < num_entries - 1; j++)
        entries[j] = entries[j + 1];

    num_entries--;
    if (num_entries)
        entries = ec_realloc(entries, sizeof(PasswdEntry) * num_entries);
    else {
        free(entries);
        entries = NULL;
    }

    if (!storage_write_user_vault()) {
        util_log(LOG_ERROR, "Failed to write updated user vault");
        return false;
    }

    return true;
}

bool update_entry(int id, PasswdEntry *new_entry) {

    int index = -1;
    for (int i = 0; i < num_entries; i++) {
        if (entries[i].id == id) {
            index = i;
            continue;
        }

        if (!strcmp(entries[i].service, new_entry->service)) {
            util_log(LOG_ERROR, "Duplicate service name");
            return false;
        }
    }

    if (index < 0) {
        util_log(LOG_ERROR, "Failed to update entry: id not found");
        return false;
    }

    entries[index].service  = strdup(new_entry->service);
    entries[index].username = strdup(new_entry->username);
    entries[index].password = strdup(new_entry->password);
    entries[index].notes    = strdup(new_entry->notes);

    qsort(entries, num_entries, sizeof(PasswdEntry), compare_entries_by_service);

    if (!storage_write_user_vault()) {
        util_log(LOG_ERROR, "Failed to update user vault; edits will not be saved to disk");
        return false;
    }

    return true;
}

// Shred the sensitive entries array
void wipe_passwd_entries(PasswdEntry *entries, int num_entries) {
    for (int i = 0; i < num_entries; i++) {

        if (entries[i].service) {
            wipe_mem(entries[i].service, strlen(entries[i].service));
            free(entries[i].service);
            entries[i].service = NULL;
        }

        if (entries[i].username) {
            wipe_mem(entries[i].username, strlen(entries[i].username));
            free(entries[i].username);
            entries[i].username = NULL;
        }

        if (entries[i].password) {
            wipe_mem(entries[i].password, strlen(entries[i].password));
            free(entries[i].password);
            entries[i].password = NULL;
        }

        if (entries[i].notes) {
            wipe_mem(entries[i].notes, strlen(entries[i].notes));
            free(entries[i].notes);
            entries[i].notes = NULL;
        }
    }

    wipe_mem(entries, sizeof(PasswdEntry) * num_entries);
    free(entries);
}