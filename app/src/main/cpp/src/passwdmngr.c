#include "../include/crypto.h"
#include "../include/storage.h"
#include "../include/util.h"
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#define GET_STR(jstring) (*env)->GetStringUTFChars(env, jstring, 0)
#define FREE_STR(jstring, string) (*env)->ReleaseStringUTFChars(env, jstring, string)

uint8_t *get_bytes(JNIEnv *env, jbyteArray jbytes, int *len)
{
    jbyte *buf = (*env)->GetByteArrayElements(env, jbytes, NULL);
    *len = (*env)->GetArrayLength(env, jbytes);
    return (uint8_t *)buf;
}
#define GET_BYTES(jbytes, len) get_bytes(env, jbytes, len)
#define FREE_BYTES(jbytes, buf) (*env)->ReleaseByteArrayElements(env, jbytes, (jbyte *)buf, JNI_ABORT)

jobject build_vault_read_result(JNIEnv *env, PasswdEntry *out_entries, VaultHeader *out_hdr)
{
    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    jmethodID entryCtor = (*env)->GetMethodID(env, entryCls, "<init>", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jobjectArray jentries = (*env)->NewObjectArray(env, out_hdr->num_entries, entryCls, NULL);
    for (int i = 0; i < out_hdr->num_entries; i++)
    {
        PasswdEntry *e = &out_entries[i];
        jstring jservice = e->service ? (*env)->NewStringUTF(env, e->service) : NULL;
        jstring jusername = e->username ? (*env)->NewStringUTF(env, e->username) : NULL;
        jstring jpassword = e->password ? (*env)->NewStringUTF(env, e->password) : NULL;
        jstring jnotes = e->notes ? (*env)->NewStringUTF(env, e->notes) : NULL;
        jobject jentry = (*env)->NewObject(env, entryCls, entryCtor, (jint)e->id, jservice, jusername, jpassword, jnotes);
        (*env)->SetObjectArrayElement(env, jentries, i, jentry);
    }

    jclass hdrCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/VaultHeader");
    jmethodID hdrCtor = (*env)->GetMethodID(env, hdrCls, "<init>", "([BI[BJII[B[B[B)V");

    jbyteArray jmagic = (*env)->NewByteArray(env, 6);
    (*env)->SetByteArrayRegion(env, jmagic, 0, 6, (jbyte *)out_hdr->magic);

    jbyteArray jhash = (*env)->NewByteArray(env, HASH_LEN);
    (*env)->SetByteArrayRegion(env, jhash, 0, HASH_LEN, (jbyte *)out_hdr->hash);

    jbyteArray jsalt = (*env)->NewByteArray(env, SALT_LEN);
    (*env)->SetByteArrayRegion(env, jsalt, 0, SALT_LEN, (jbyte *)out_hdr->salt);

    jbyteArray jnonce = (*env)->NewByteArray(env, NONCE_LEN);
    (*env)->SetByteArrayRegion(env, jnonce, 0, NONCE_LEN, (jbyte *)out_hdr->nonce);

    jbyteArray jtag = (*env)->NewByteArray(env, TAG_LEN);
    (*env)->SetByteArrayRegion(env, jtag, 0, TAG_LEN, (jbyte *)out_hdr->tag);

    jobject jhdr = (*env)->NewObject(env, hdrCls, hdrCtor,
                                     jmagic,
                                     (jint)out_hdr->version,
                                     jhash,
                                     (jlong)out_hdr->timestamp,
                                     (jint)out_hdr->num_entries,
                                     (jint)out_hdr->ciphertext_len,
                                     jsalt,
                                     jnonce,
                                     jtag);

    jclass resultCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/VaultReadResult");
    jmethodID resultCtor = (*env)->GetMethodID(env, resultCls, "<init>",
                                               "([Lcom/samuelf09/passwdmngr/PasswdEntry;Lcom/samuelf09/passwdmngr/VaultHeader;)V");

    return (*env)->NewObject(env, resultCls, resultCtor, jentries, jhdr);
}

// CRYPTO.H

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_verifyAccount(JNIEnv *env, jclass clazz, jstring juname,
                                                   jstring jpasswd)
{
    const char *uname = GET_STR(juname);
    const char *passwd = GET_STR(jpasswd);

    bool result = verify_account(uname, passwd);

    FREE_STR(juname, uname);
    FREE_STR(jpasswd, passwd);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_verifyPwHash(JNIEnv *env, jclass clazz, jbyteArray jhash,
                                                  jstring jpasswd)
{
    int hash_len;
    uint8_t *hash = GET_BYTES(jhash, &hash_len);
    char *passwd = (char *)GET_STR(jpasswd);

    if (hash_len != HASH_LEN)
    {
        util_log(LOG_ERROR, "Invalid hash length!");
        FREE_BYTES(jhash, hash);
        FREE_STR(jpasswd, passwd);
        return false;
    }

    bool result = verify_pwhash(hash, passwd);

    FREE_BYTES(jhash, hash);
    FREE_STR(jpasswd, passwd);

    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_samuelf09_passwdmngr_Native_hashPwWithSalt(JNIEnv *env, jclass clazz, jstring jpasswd,
                                                    jbyteArray jsalt)
{
    const char *passwd = GET_STR(jpasswd);
    int salt_len;
    uint8_t *salt = GET_BYTES(jsalt, &salt_len);

    uint8_t *hash = ec_malloc(HASH_LEN + SALT_LEN);
    if (!hash_pw_with_salt(passwd, hash, HASH_LEN + SALT_LEN, salt))
    {
        util_log(LOG_ERROR, "hash_pw_with_salt failed!");
        FREE_STR(jpasswd, passwd);
        FREE_BYTES(jsalt, salt);
        return NULL;
    }

    FREE_STR(jpasswd, passwd);
    FREE_BYTES(jsalt, salt);

    jbyteArray out = (*env)->NewByteArray(env, HASH_LEN + SALT_LEN);
    (*env)->SetByteArrayRegion(env, out, 0, HASH_LEN + SALT_LEN, (jbyte *)hash);
    free(hash);
    return out;
}

JNIEXPORT jbyteArray JNICALL
Java_com_samuelf09_passwdmngr_Native_hashPw(JNIEnv *env, jclass clazz, jstring jpasswd)
{

    const char *passwd = GET_STR(jpasswd);

    uint8_t *hash = ec_malloc(HASH_LEN + SALT_LEN);
    if (!hash_pw(passwd, hash, HASH_LEN + SALT_LEN))
    {
        util_log(LOG_ERROR, "hash_pw failed!");
        FREE_STR(jpasswd, passwd);
        return NULL;
    }

    FREE_STR(jpasswd, passwd);

    jbyteArray out = (*env)->NewByteArray(env, HASH_LEN + SALT_LEN);
    (*env)->SetByteArrayRegion(env, out, 0, HASH_LEN + SALT_LEN, (jbyte *)hash);
    free(hash);
    return out;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_hashUname(JNIEnv *env, jclass clazz, jstring juname)
{

    const char *uname = GET_STR(juname);

    char *hash = hash_uname(uname);
    FREE_STR(juname, uname);
    if (!util_check_ptr(hash, "hash_uname failed!"))
        return NULL;

    jstring jhash = (*env)->NewStringUTF(env, hash);
    return jhash;
}

JNIEXPORT jbyteArray JNICALL
Java_com_samuelf09_passwdmngr_Native_sha256Hash(JNIEnv *env, jclass clazz, jbyteArray jdata)
{

    int data_len;
    uint8_t *data = GET_BYTES(jdata, &data_len);

    uint8_t *hash = sha_256_hash(data, data_len);
    FREE_BYTES(jdata, data);
    if (!util_check_ptr(hash, "sha_256_hash failed!"))
        return NULL;

    jbyteArray out = (*env)->NewByteArray(env, HASH_LEN);
    (*env)->SetByteArrayRegion(env, out, 0, HASH_LEN, (jbyte *)hash);
    free(hash);
    return out;
}

JNIEXPORT jbyteArray JNICALL
Java_com_samuelf09_passwdmngr_Native_deriveVaultKey(JNIEnv *env, jclass clazz, jstring jpasswd,
                                                    jbyteArray jsalt)
{
    const char *passwd = GET_STR(jpasswd);
    int salt_len;
    uint8_t *salt = GET_BYTES(jsalt, &salt_len);

    if (salt_len != SALT_LEN)
    {
        util_log(LOG_ERROR, "Invalid salt length!");
        FREE_STR(jpasswd, passwd);
        FREE_BYTES(jsalt, salt);
        return NULL;
    }

    uint8_t key[HASH_LEN];
    if (!derive_vault_key(passwd, salt, key, sizeof(key)))
    {
        util_log(LOG_ERROR, "derive_vault_key failed!");
        FREE_STR(jpasswd, passwd);
        FREE_BYTES(jsalt, salt);
        return NULL;
    }

    jbyteArray out = (*env)->NewByteArray(env, HASH_LEN);
    (*env)->SetByteArrayRegion(env, out, 0, HASH_LEN, (jbyte *)key);
    return out;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_genPasswd(JNIEnv *env, jclass clazz, jint jlen, jstring jspecial,
                                               jboolean jdigits, jboolean juppers, jboolean jlowers)
{
    int len = (int)jlen;
    char *special = (char *)GET_STR(jspecial);
    bool digits = jdigits;
    bool uppers = juppers;
    bool lowers = jlowers;

    char *pass = gen_passwd(len, special, digits, uppers, lowers);
    FREE_STR(jspecial, special);
    if (!util_check_ptr(pass, "gen_passwd failed!"))
        return NULL;

    jstring jpass = (*env)->NewStringUTF(env, pass);
    free(pass);
    return jpass;
}

JNIEXPORT jobject JNICALL
Java_com_samuelf09_passwdmngr_Native_aesGcmEncrypt(JNIEnv *env, jclass clazz, jbyteArray jplaintext,
                                                   jbyteArray jkey, jbyteArray jiv)
{
    int plaintext_len;
    int key_len;
    int iv_len;
    uint8_t *plaintext = GET_BYTES(jplaintext, &plaintext_len);
    uint8_t *key = GET_BYTES(jkey, &key_len);
    uint8_t *iv = GET_BYTES(jiv, &iv_len);

    uint8_t *ciphertext = ec_malloc(plaintext_len + 16);
    uint8_t *tag = ec_malloc(TAG_LEN);

    int ciphertext_len = aes_gcm_encrypt(plaintext, plaintext_len, key, iv, iv_len, ciphertext, tag);
    FREE_BYTES(jplaintext, plaintext);
    FREE_BYTES(jkey, key);
    FREE_BYTES(jiv, iv);

    if (ciphertext < 0)
    {
        util_log(LOG_ERROR, "aes_gcm_encrypt returned %d!", ciphertext_len);
        return NULL;
    }

    // make EncryptionResult return object
    jbyteArray jciphertext = (*env)->NewByteArray(env, ciphertext_len);
    (*env)->SetByteArrayRegion(env, jciphertext, 0, ciphertext_len, (jbyte *)ciphertext);
    jbyteArray jtag = (*env)->NewByteArray(env, TAG_LEN);
    (*env)->SetByteArrayRegion(env, jtag, 0, TAG_LEN, (jbyte *)tag);

    free(ciphertext);
    free(tag);

    jclass cls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/EncryptionResult");
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "([B[B)V");
    jobject result = (*env)->NewObject(env, cls, ctor, jciphertext, jtag);

    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_samuelf09_passwdmngr_Native_aesGcmDecrypt(JNIEnv *env, jclass clazz, jbyteArray jciphertext,
                                                   jbyteArray jkey, jbyteArray jiv, jbyteArray jtag)
{
    int ciphertext_len;
    int key_len;
    int iv_len;
    int tag_len;
    uint8_t *ciphertext = GET_BYTES(jciphertext, &ciphertext_len);
    uint8_t *key = GET_BYTES(jkey, &key_len);
    uint8_t *iv = GET_BYTES(jiv, &iv_len);
    uint8_t *tag = GET_BYTES(jtag, &tag_len);

    uint8_t *plaintext = ec_malloc(ciphertext_len);
    int plaintext_len = aes_gcm_decrypt(ciphertext, ciphertext_len, key, iv, iv_len, tag, plaintext);
    FREE_BYTES(jciphertext, ciphertext);
    FREE_BYTES(jkey, key);
    FREE_BYTES(jiv, iv);
    FREE_BYTES(jtag, tag);

    if (plaintext_len < 0)
    {
        util_log(LOG_ERROR, "aes_gcm_decrypt failed!");
        free(plaintext);
        return NULL;
    }

    jbyteArray out = (*env)->NewByteArray(env, plaintext_len);
    (*env)->SetByteArrayRegion(env, out, 0, plaintext_len, (jbyte *)plaintext);
    free(plaintext);
    return out;
}

// STORAGE.H

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_loadAccounts(
    JNIEnv *env, jclass clazz)
{
    return load_accounts();
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_initAccounts(
    JNIEnv *env, jclass clazz)
{
    return init_accounts();
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_initPrefJson(
    JNIEnv *env, jclass clazz, jstring jprefPath)
{
    char *pref_path = (char *)GET_STR(jprefPath);
    init_pref_json(pref_path);
    FREE_STR(jprefPath, pref_path);
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_saveAccounts(
    JNIEnv *env, jclass clazz)
{
    save_accounts();
}

JNIEXPORT jobjectArray JNICALL
Java_com_samuelf09_passwdmngr_Native_storageReadPrefs(
    JNIEnv *env, jclass clazz)
{
    UserPref *prefs = NULL;
    int num_prefs = storage_read_prefs(&prefs);
    if (num_prefs < 0)
    {
        util_log(LOG_ERROR, "Failed to fetch user prefs!");
        return NULL;
    }

    jclass prefCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/UserPref");
    if (!prefCls)
    {
        util_log(LOG_ERROR, "Failed to find UserPref class!");
        return NULL;
    }

    jmethodID ctor = (*env)->GetMethodID(env, prefCls, "<init>", "(Ljava/lang/String;)V");
    if (!ctor)
    {
        util_log(LOG_ERROR, "Failed to find UserPref constructor!");
        return NULL;
    }

    jobjectArray arr = (*env)->NewObjectArray(env, num_prefs, prefCls, NULL);
    if (!arr)
    {
        util_log(LOG_ERROR, "Failed to allocate UserPref array!");
        return NULL;
    }

    for (int i = 0; i < num_prefs; i++)
    {
        char *hash = prefs[i].uname_hash;

        jstring jhash = NULL;
        if (hash)
            jhash = (*env)->NewStringUTF(env, hash);

        jobject prefObj = (*env)->NewObject(env, prefCls, ctor, jhash);
        (*env)->SetObjectArrayElement(env, arr, i, prefObj);
    }

    return arr;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_storageSavePrefs(
    JNIEnv *env, jclass clazz, jobjectArray jprefs)
{

    if (!jprefs)
        return false;

    jsize count = (*env)->GetArrayLength(env, jprefs);
    jclass prefCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/UserPref");
    jfieldID unameHashField = (*env)->GetFieldID(env, prefCls,
                                                 "unameHash",
                                                 "Ljava/lang/String;");

    UserPref *prefs = ec_calloc(count, sizeof(UserPref));

    for (jsize i = 0; i < count; i++)
    {
        jobject prefObj = (*env)->GetObjectArrayElement(env, jprefs, i);
        jstring jhash = (jstring)(*env)->GetObjectField(env, prefObj, unameHashField);

        const char *hash = NULL;
        if (jhash)
            hash = GET_STR(jhash);

        prefs[i].uname_hash = hash ? strdup(hash) : NULL;

        if (jhash)
            FREE_STR(jhash, hash);
    }

    bool result = storage_save_prefs(prefs, count);

    for (jsize i = 0; i < count; i++)
        free(prefs[i].uname_hash);
    free(prefs);

    return result;
}

JNIEXPORT jobject JNICALL
Java_com_samuelf09_passwdmngr_Native_getUserPrefs(
    JNIEnv *env, jclass clazz, jstring jusername)
{

    char *uname = (char *)GET_STR(jusername);
    UserPref *pref = get_user_prefs(uname);
    FREE_STR(jusername, uname);

    jclass prefCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/UserPref");
    if (!prefCls)
    {
        util_log(LOG_ERROR, "Failed to find UserPref class!");
        return NULL;
    }
    jmethodID ctor = (*env)->GetMethodID(env, prefCls, "<init>", "(Ljava/lang/String;)V");
    if (!ctor)
    {
        util_log(LOG_ERROR, "Failed to find UserPref constructor!");
        return NULL;
    }

    char *hash = pref->uname_hash;
    jstring jhash = hash ? (*env)->NewStringUTF(env, hash) : NULL;
    jobject prefObj = (*env)->NewObject(env, prefCls, ctor, jhash);
    return prefObj;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_createNewAccount(
    JNIEnv *env, jclass clazz, jstring juname, jstring jpassword)
{

    char *uname = (char *)GET_STR(juname);
    char *password = (char *)GET_STR(jpassword);

    bool result = create_new_account(uname, password);

    FREE_STR(juname, uname);
    FREE_STR(jpassword, password);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_storageDeleteAccount(
    JNIEnv *env, jclass clazz, jstring juname)
{

    char *uname = (char *)GET_STR(juname);

    bool result = storage_delete_account(uname);

    FREE_STR(juname, uname);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_storageChangePasswd(
    JNIEnv *env, jclass clazz, jstring juname, jstring jnewPass)
{

    char *uname = (char *)GET_STR(juname);
    char *new_pass = (char *)GET_STR(jnewPass);

    bool result = storage_change_passwd(uname, new_pass);

    FREE_STR(juname, uname);
    FREE_STR(jnewPass, new_pass);

    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_com_samuelf09_passwdmngr_Native_getUserSalt(
    JNIEnv *env, jclass clazz)
{

    uint8_t *salt = get_user_salt();

    if (!salt)
        return NULL;

    jbyteArray out = (*env)->NewByteArray(env, SALT_LEN);
    (*env)->SetByteArrayRegion(env, out, 0, SALT_LEN, (jbyte *)salt);
    free(salt);
    return out;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_storageGetUserVaultPath(
    JNIEnv *env, jclass clazz, jstring juname)
{

    char *uname = (char *)GET_STR(juname);

    char *vp = storage_get_user_vault_path(uname);
    FREE_STR(juname, uname);

    jstring jvp = vp ? (*env)->NewStringUTF(env, vp) : NULL;
    if (vp)
        free(vp);

    return jvp;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_storageDumpJson(
    JNIEnv *env, jclass clazz,
    jstring jvaultPath,
    jbyteArray jkey,
    jboolean jpretty)
{

    int key_len;
    char *vault_path = (char *)GET_STR(jvaultPath);
    uint8_t *key = GET_BYTES(jkey, &key_len);
    bool pretty = (bool)jpretty;

    char *dump = NULL;
    int dump_len = storage_dump_json(vault_path, &dump, key, pretty);
    FREE_STR(jvaultPath, vault_path);
    FREE_BYTES(jkey, key);
    if (dump_len <= 0)
    {
        util_log(LOG_ERROR, "storage_dump_json returned %d!", dump_len);
        return NULL;
    }

    jstring jdump = dump ? (*env)->NewStringUTF(env, dump) : NULL;
    if (dump)
        free(dump);

    return dump;
}

JNIEXPORT jobject JNICALL
Java_com_samuelf09_passwdmngr_Native_encryptEntries(
    JNIEnv *env, jclass clazz,
    jobjectArray jentries,
    jbyteArray jsalt)
{

    // derives key from username

    int num_encrypt_entries = (*env)->GetArrayLength(env, jentries);
    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    jfieldID idField = (*env)->GetFieldID(env, entryCls, "id", "I");
    jfieldID serviceField = (*env)->GetFieldID(env, entryCls, "service", "Ljava/lang/String;");
    jfieldID usernameField = (*env)->GetFieldID(env, entryCls, "username", "Ljava/lang/String;");
    jfieldID passwordField = (*env)->GetFieldID(env, entryCls, "password", "Ljava/lang/String;");
    jfieldID notesField = (*env)->GetFieldID(env, entryCls, "notes", "Ljava/lang/String;");

    PasswdEntry *encrypt_entries_ = ec_calloc(num_encrypt_entries, sizeof(PasswdEntry));

    for (jsize i = 0; i < num_encrypt_entries; i++)
    {
        jobject entryObj = (*env)->GetObjectArrayElement(env, jentries, i);
        int e_id = (int)(*env)->GetIntField(env, entryObj, idField);
        jstring jservice = (jstring)(*env)->GetObjectField(env, entryObj, serviceField);
        jstring jusername = (jstring)(*env)->GetObjectField(env, entryObj, usernameField);
        jstring jpassword = (jstring)(*env)->GetObjectField(env, entryObj, passwordField);
        jstring jnotes = (jstring)(*env)->GetObjectField(env, entryObj, notesField);

        char *e_service = NULL;
        char *e_username = NULL;
        char *e_password = NULL;
        char *e_notes = NULL;
        if (jservice)
            e_service = (char *)GET_STR(jservice);
        if (jusername)
            e_username = (char *)GET_STR(jusername);
        if (jpassword)
            e_password = (char *)GET_STR(jpassword);
        if (jnotes)
            e_notes = (char *)GET_STR(jnotes);

        encrypt_entries_[i].id = e_id;
        encrypt_entries_[i].service = e_service ? strdup(e_service) : NULL;
        encrypt_entries_[i].username = e_username ? strdup(e_username) : NULL;
        encrypt_entries_[i].password = e_password ? strdup(e_password) : NULL;
        encrypt_entries_[i].notes = e_notes ? strdup(e_notes) : NULL;

        if (jservice)
            FREE_STR(jservice, e_service);
        if (jusername)
            FREE_STR(jusername, e_username);
        if (jpassword)
            FREE_STR(jpassword, e_password);
        if (jnotes)
            FREE_STR(jnotes, e_notes);
    }

    int salt_len;
    uint8_t *salt = GET_BYTES(jsalt, &salt_len);

    uint8_t *ct = NULL;
    uint8_t *nonce = NULL;
    uint8_t *tag = NULL;

    int ct_len = 0;

    if (!encrypt_entries(encrypt_entries_, num_encrypt_entries, salt, &ct, &ct_len, &nonce, &tag))
    {
        util_log(LOG_ERROR, "encrypt_entries failed!");
        FREE_BYTES(jsalt, salt);
        return NULL;
    }

    jbyteArray jct = (*env)->NewByteArray(env, ct_len);
    jbyteArray jnonce = (*env)->NewByteArray(env, NONCE_LEN);
    jbyteArray jtag = (*env)->NewByteArray(env, TAG_LEN);
    (*env)->SetByteArrayRegion(env, jct, 0, ct_len, (jbyte *)ct);
    (*env)->SetByteArrayRegion(env, jnonce, 0, NONCE_LEN, (jbyte *)nonce);
    (*env)->SetByteArrayRegion(env, jtag, 0, TAG_LEN, (jbyte *)tag);
    free(ct);
    free(nonce);
    free(tag);

    jclass cls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/EncryptEntriesResult");
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "([B[B[B)V");
    jobject result = (*env)->NewObject(env, cls, ctor, jct, jnonce, jtag);

    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_samuelf09_passwdmngr_Native_decryptEntriesWithKey(
    JNIEnv *env, jclass clazz,
    jbyteArray jkey,
    jbyteArray jciphertext,
    jbyteArray jnonce,
    jbyteArray jtag)
{

    int key_len;
    int ciphertext_len;
    int nonce_len;
    int tag_len;
    uint8_t *key = GET_BYTES(jkey, &key_len);
    uint8_t *ciphertext = GET_BYTES(jciphertext, &ciphertext_len);
    uint8_t *nonce = GET_BYTES(jnonce, &nonce_len);
    uint8_t *tag = GET_BYTES(jtag, &tag_len);

    PasswdEntry *decrypted_entries = NULL;
    int num_decrypted_entries = -1;

    if (!decrypt_entries_with_key(key, ciphertext, ciphertext_len, nonce, tag, &decrypted_entries, &num_decrypted_entries))
    {
        util_log(LOG_ERROR, "decrypt_entries_with_key failed!");
        FREE_BYTES(jkey, key);
        FREE_BYTES(jciphertext, ciphertext);
        FREE_BYTES(jnonce, nonce);
        FREE_BYTES(jtag, tag);
        return NULL;
    }

    FREE_BYTES(jkey, key);
    FREE_BYTES(jciphertext, ciphertext);
    FREE_BYTES(jnonce, nonce);
    FREE_BYTES(jtag, tag);

    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    if (!entryCls)
    {
        util_log(LOG_ERROR, "Failed to find PasswdEntry class!");
        return NULL;
    }

    jmethodID ctor = (*env)->GetMethodID(env, entryCls, "<init>",
                                         "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor)
    {
        util_log(LOG_ERROR, "Failed to find PasswdEntry constructor!");
        return NULL;
    }

    jobjectArray arr = (*env)->NewObjectArray(env, num_decrypted_entries, entryCls, NULL);
    if (!arr)
    {
        util_log(LOG_ERROR, "Failed to allocate PasswdEntry array!");
        return NULL;
    }

    for (int i = 0; i < num_decrypted_entries; i++)
    {
        int e_id = decrypted_entries[i].id;
        char *e_service = decrypted_entries[i].service;
        char *e_username = decrypted_entries[i].username;
        char *e_password = decrypted_entries[i].password;
        char *e_notes = decrypted_entries[i].notes;

        jstring jservice = NULL;
        jstring jusername = NULL;
        jstring jpassword = NULL;
        jstring jnotes = NULL;
        if (e_service)
            jservice = (*env)->NewStringUTF(env, e_service);
        if (e_username)
            jusername = (*env)->NewStringUTF(env, e_username);
        if (e_password)
            jpassword = (*env)->NewStringUTF(env, e_password);
        if (e_notes)
            jnotes = (*env)->NewStringUTF(env, e_notes);

        jobject entryObj = (*env)->NewObject(env, entryCls, ctor, (jint)e_id, jservice, jusername, jpassword, jnotes);
        (*env)->SetObjectArrayElement(env, arr, i, entryObj);
    }

    wipe_passwd_entries(decrypted_entries, num_decrypted_entries);

    return arr;
}

JNIEXPORT jobjectArray JNICALL
Java_com_samuelf09_passwdmngr_Native_decryptEntries(
    JNIEnv *env, jclass clazz,
    jbyteArray jsalt,
    jbyteArray jciphertext,
    jbyteArray jnonce,
    jbyteArray jtag)
{
    int salt_len;
    int ciphertext_len;
    int nonce_len;
    int tag_len;
    uint8_t *salt = GET_BYTES(jsalt, &salt_len);
    uint8_t *ciphertext = GET_BYTES(jciphertext, &ciphertext_len);
    uint8_t *nonce = GET_BYTES(jnonce, &nonce_len);
    uint8_t *tag = GET_BYTES(jtag, &tag_len);

    PasswdEntry *decrypted_entries = NULL;
    int num_decrypted_entries = -1;

    if (!decrypt_entries(salt, ciphertext, ciphertext_len, nonce, tag, &decrypted_entries, &num_decrypted_entries))
    {
        util_log(LOG_ERROR, "decrypt_entries failed!");
        FREE_BYTES(jsalt, salt);
        FREE_BYTES(jciphertext, ciphertext);
        FREE_BYTES(jnonce, nonce);
        FREE_BYTES(jtag, tag);
        return NULL;
    }

    FREE_BYTES(jsalt, salt);
    FREE_BYTES(jciphertext, ciphertext);
    FREE_BYTES(jnonce, nonce);
    FREE_BYTES(jtag, tag);

    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    if (!entryCls)
    {
        util_log(LOG_ERROR, "Failed to find PasswdEntry class!");
        return NULL;
    }

    jmethodID ctor = (*env)->GetMethodID(env, entryCls, "<init>",
                                         "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor)
    {
        util_log(LOG_ERROR, "Failed to find PasswdEntry constructor!");
        return NULL;
    }

    jobjectArray arr = (*env)->NewObjectArray(env, num_decrypted_entries, entryCls, NULL);
    if (!arr)
    {
        util_log(LOG_ERROR, "Failed to allocate PasswdEntry array!");
        return NULL;
    }

    for (int i = 0; i < num_decrypted_entries; i++)
    {
        int e_id = decrypted_entries[i].id;
        char *e_service = decrypted_entries[i].service;
        char *e_username = decrypted_entries[i].username;
        char *e_password = decrypted_entries[i].password;
        char *e_notes = decrypted_entries[i].notes;

        jstring jservice = NULL;
        jstring jusername = NULL;
        jstring jpassword = NULL;
        jstring jnotes = NULL;
        if (e_service)
            jservice = (*env)->NewStringUTF(env, e_service);
        if (e_username)
            jusername = (*env)->NewStringUTF(env, e_username);
        if (e_password)
            jpassword = (*env)->NewStringUTF(env, e_password);
        if (e_notes)
            jnotes = (*env)->NewStringUTF(env, e_notes);

        jobject entryObj = (*env)->NewObject(env, entryCls, ctor, (jint)e_id, jservice, jusername, jpassword, jnotes);
        (*env)->SetObjectArrayElement(env, arr, i, entryObj);
    }

    wipe_passwd_entries(decrypted_entries, num_decrypted_entries);

    return arr;
}

JNIEXPORT jobject JNICALL
Java_com_samuelf09_passwdmngr_Native_storageReadVaultWithKey(
    JNIEnv *env, jclass clazz,
    jstring jvaultPath,
    jbyteArray jkey)
{
    char *vp = (char *)GET_STR(jvaultPath);
    int key_len;
    uint8_t *key = GET_BYTES(jkey, &key_len);

    PasswdEntry *out_entries = NULL;
    VaultHeader *out_hdr = NULL;

    int ret = storage_read_vault_with_key(vp, key, &out_entries, &out_hdr);
    FREE_STR(jvaultPath, vp);
    FREE_BYTES(jkey, key);

    if (ret < 0)
    {
        util_log(LOG_ERROR, "storage_read_vault_with_key failed with code %d!", ret);
        return NULL;
    }

    jobject result = build_vault_read_result(env, out_entries, out_hdr);

    wipe_passwd_entries(out_entries, out_hdr->num_entries);
    free(out_hdr);

    return result;
}

JNIEXPORT jobject JNICALL
Java_com_samuelf09_passwdmngr_Native_storageReadVault(
    JNIEnv *env, jclass clazz,
    jstring jvaultPath)
{
    char *vp = (char *)GET_STR(jvaultPath);

    PasswdEntry *out_entries = NULL;
    VaultHeader *out_hdr = NULL;

    int ret = storage_read_vault(vp, &out_entries, &out_hdr);
    FREE_STR(jvaultPath, vp);

    if (ret < 0)
    {
        util_log(LOG_ERROR, "storage_read_vault failed with code %d!", ret);
        return NULL;
    }

    jobject result = build_vault_read_result(env, out_entries, out_hdr);

    wipe_passwd_entries(out_entries, out_hdr->num_entries);
    free(out_hdr);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_storageReadUserVault(
    JNIEnv *env, jclass clazz)
{
    return storage_read_user_vault();
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_storageWriteVault(
    JNIEnv *env, jclass clazz,
    jstring jvaultPath,
    jobjectArray jentries,
    jbyteArray jsalt)
{
    char *vp = (char *)GET_STR(jvaultPath);

    int num_write_entries = (*env)->GetArrayLength(env, jentries);
    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    jfieldID idField = (*env)->GetFieldID(env, entryCls, "id", "I");
    jfieldID serviceField = (*env)->GetFieldID(env, entryCls, "service", "Ljava/lang/String;");
    jfieldID usernameField = (*env)->GetFieldID(env, entryCls, "username", "Ljava/lang/String;");
    jfieldID passwordField = (*env)->GetFieldID(env, entryCls, "password", "Ljava/lang/String;");
    jfieldID notesField = (*env)->GetFieldID(env, entryCls, "notes", "Ljava/lang/String;");

    PasswdEntry *write_entries = ec_calloc(num_write_entries, sizeof(PasswdEntry));

    for (jsize i = 0; i < num_write_entries; i++)
    {
        jobject entryObj = (*env)->GetObjectArrayElement(env, jentries, i);
        int e_id = (int)(*env)->GetIntField(env, entryObj, idField);
        jstring jservice = (jstring)(*env)->GetObjectField(env, entryObj, serviceField);
        jstring jusername = (jstring)(*env)->GetObjectField(env, entryObj, usernameField);
        jstring jpassword = (jstring)(*env)->GetObjectField(env, entryObj, passwordField);
        jstring jnotes = (jstring)(*env)->GetObjectField(env, entryObj, notesField);

        char *e_service = NULL;
        char *e_username = NULL;
        char *e_password = NULL;
        char *e_notes = NULL;
        if (jservice)
            e_service = (char *)GET_STR(jservice);
        if (jusername)
            e_username = (char *)GET_STR(jusername);
        if (jpassword)
            e_password = (char *)GET_STR(jpassword);
        if (jnotes)
            e_notes = (char *)GET_STR(jnotes);

        write_entries[i].id = e_id;
        write_entries[i].service = e_service ? strdup(e_service) : NULL;
        write_entries[i].username = e_username ? strdup(e_username) : NULL;
        write_entries[i].password = e_password ? strdup(e_password) : NULL;
        write_entries[i].notes = e_notes ? strdup(e_notes) : NULL;

        if (jservice)
            FREE_STR(jservice, e_service);
        if (jusername)
            FREE_STR(jusername, e_username);
        if (jpassword)
            FREE_STR(jpassword, e_password);
        if (jnotes)
            FREE_STR(jnotes, e_notes);
    }

    int salt_len;
    uint8_t *salt = GET_BYTES(jsalt, &salt_len);

    bool res = storage_write_vault(vp, write_entries, num_write_entries, salt);

    wipe_passwd_entries(write_entries, num_write_entries);
    FREE_STR(jvaultPath, vp);
    FREE_BYTES(jsalt, salt);

    return res;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_storageWriteUserVault(
    JNIEnv *env, jclass clazz)
{
    return storage_write_user_vault();
}

JNIEXPORT jint JNICALL
Java_com_samuelf09_passwdmngr_Native_storageGetNextId(
    JNIEnv *env, jclass clazz)
{
    return (jint)storage_get_next_id();
}

JNIEXPORT jobject JNICALL
Java_com_samuelf09_passwdmngr_Native_storageGetEntry(
    JNIEnv *env, jclass clazz, jint jid)
{
    PasswdEntry *e = storage_get_entry((int)jid);
    if (!e)
        return NULL;

    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    jmethodID entryCtor = (*env)->GetMethodID(env, entryCls, "<init>", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jstring jservice = e->service ? (*env)->NewStringUTF(env, e->service) : NULL;
    jstring jusername = e->username ? (*env)->NewStringUTF(env, e->username) : NULL;
    jstring jpassword = e->password ? (*env)->NewStringUTF(env, e->password) : NULL;
    jstring jnotes = e->notes ? (*env)->NewStringUTF(env, e->notes) : NULL;
    jobject jentry = (*env)->NewObject(env, entryCls, entryCtor, (jint)e->id, jservice, jusername, jpassword, jnotes);
    return jentry;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_addEntry(
    JNIEnv *env, jclass clazz, jobject jentry)
{
    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    jfieldID idField = (*env)->GetFieldID(env, entryCls, "id", "I");
    jfieldID serviceField = (*env)->GetFieldID(env, entryCls, "service", "Ljava/lang/String;");
    jfieldID usernameField = (*env)->GetFieldID(env, entryCls, "username", "Ljava/lang/String;");
    jfieldID passwordField = (*env)->GetFieldID(env, entryCls, "password", "Ljava/lang/String;");
    jfieldID notesField = (*env)->GetFieldID(env, entryCls, "notes", "Ljava/lang/String;");

    PasswdEntry *e = ec_calloc(1, sizeof(PasswdEntry));
    int e_id = (int)(*env)->GetIntField(env, jentry, idField);
    jstring jservice = (jstring)(*env)->GetObjectField(env, jentry, serviceField);
    jstring jusername = (jstring)(*env)->GetObjectField(env, jentry, usernameField);
    jstring jpassword = (jstring)(*env)->GetObjectField(env, jentry, passwordField);
    jstring jnotes = (jstring)(*env)->GetObjectField(env, jentry, notesField);

    char *e_service = NULL;
    char *e_username = NULL;
    char *e_password = NULL;
    char *e_notes = NULL;
    if (jservice)
        e_service = (char *)GET_STR(jservice);
    if (jusername)
        e_username = (char *)GET_STR(jusername);
    if (jpassword)
        e_password = (char *)GET_STR(jpassword);
    if (jnotes)
        e_notes = (char *)GET_STR(jnotes);

    e->id = e_id;
    e->service = e_service ? strdup(e_service) : NULL;
    e->username = e_username ? strdup(e_username) : NULL;
    e->password = e_password ? strdup(e_password) : NULL;
    e->notes = e_notes ? strdup(e_notes) : NULL;

    if (jservice)
        FREE_STR(jservice, e_service);
    if (jusername)
        FREE_STR(jusername, e_username);
    if (jpassword)
        FREE_STR(jpassword, e_password);
    if (jnotes)
        FREE_STR(jnotes, e_notes);

    bool res = add_entry(e);
    wipe_passwd_entries(e, 1);

    return res;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_deleteEntry(
    JNIEnv *env, jclass clazz, jint jid)
{
    return delete_entry((int)jid);
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_updateEntry(
    JNIEnv *env, jclass clazz, jint jid, jobject jnewEntry)
{
    jclass entryCls = (*env)->FindClass(env, "com/samuelf09/passwdmngr/PasswdEntry");
    jfieldID idField = (*env)->GetFieldID(env, entryCls, "id", "I");
    jfieldID serviceField = (*env)->GetFieldID(env, entryCls, "service", "Ljava/lang/String;");
    jfieldID usernameField = (*env)->GetFieldID(env, entryCls, "username", "Ljava/lang/String;");
    jfieldID passwordField = (*env)->GetFieldID(env, entryCls, "password", "Ljava/lang/String;");
    jfieldID notesField = (*env)->GetFieldID(env, entryCls, "notes", "Ljava/lang/String;");

    PasswdEntry *e = ec_calloc(1, sizeof(PasswdEntry));
    int e_id = (int)(*env)->GetIntField(env, jnewEntry, idField);
    jstring jservice = (jstring)(*env)->GetObjectField(env, jnewEntry, serviceField);
    jstring jusername = (jstring)(*env)->GetObjectField(env, jnewEntry, usernameField);
    jstring jpassword = (jstring)(*env)->GetObjectField(env, jnewEntry, passwordField);
    jstring jnotes = (jstring)(*env)->GetObjectField(env, jnewEntry, notesField);

    char *e_service = NULL;
    char *e_username = NULL;
    char *e_password = NULL;
    char *e_notes = NULL;
    if (jservice)
        e_service = (char *)GET_STR(jservice);
    if (jusername)
        e_username = (char *)GET_STR(jusername);
    if (jpassword)
        e_password = (char *)GET_STR(jpassword);
    if (jnotes)
        e_notes = (char *)GET_STR(jnotes);

    e->id = e_id;
    e->service = e_service ? strdup(e_service) : NULL;
    e->username = e_username ? strdup(e_username) : NULL;
    e->password = e_password ? strdup(e_password) : NULL;
    e->notes = e_notes ? strdup(e_notes) : NULL;

    if (jservice)
        FREE_STR(jservice, e_service);
    if (jusername)
        FREE_STR(jusername, e_username);
    if (jpassword)
        FREE_STR(jpassword, e_password);
    if (jnotes)
        FREE_STR(jnotes, e_notes);

    bool res = update_entry((int)jid, e);
    wipe_passwd_entries(e, 1);

    return res;
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_wipePasswdEntries(
    JNIEnv *env, jclass clazz, jobjectArray jentries)
{
    // TODO: redesign necessary to securely store secrets in native memory
}

// UTIL.H

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_utilGetAppDir(
    JNIEnv *env, jclass clazz)
{
    char *app_dir = util_get_app_dir();
    return app_dir ? (*env)->NewStringUTF(env, app_dir) : NULL;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_utilGetLogfile(
    JNIEnv *env, jclass clazz)
{
    char *logfile = util_get_logfile();
    return logfile ? (*env)->NewStringUTF(env, logfile) : NULL;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_utilGetPrefsFile(
    JNIEnv *env, jclass clazz)
{
    char *prefs_file = util_get_prefs_file();
    return prefs_file ? (*env)->NewStringUTF(env, prefs_file) : NULL;
}

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_utilGetAccountsFile(
    JNIEnv *env, jclass clazz)
{
    char *accounts_file = util_get_accounts_file();
    return accounts_file ? (*env)->NewStringUTF(env, accounts_file) : NULL;
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_utilAssert(
    JNIEnv *env, jclass clazz, jint jcond, jstring jfailMsg)
{
    char *fail_msg = (char *)GET_STR(jfailMsg);
    util_assert((int)jcond, fail_msg);
    FREE_STR(jfailMsg, fail_msg);
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_utilLog(
    JNIEnv *env, jclass clazz, jint jlevel, jstring jmsg)
{
    char *msg = (char *)GET_STR(jmsg);
    util_log((int)jlevel, msg);
    FREE_STR(jmsg, msg);
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_wipeMem(
    JNIEnv *env, jclass clazz, jbyteArray jbytes)
{
    // TODO: redesign necessary to securely store secrets in native memory
}

JNIEXPORT jint JNICALL
Java_com_samuelf09_passwdmngr_Native_countSubstrings(
    JNIEnv *env, jclass clazz, jstring jhaystack, jstring jneedle)
{
    const char *haystack = GET_STR(jhaystack);
    const char *needle = GET_STR(jneedle);
    int res = count_substrings(haystack, needle);
    FREE_STR(jhaystack, haystack);
    FREE_STR(jneedle, needle);
    return res;
}