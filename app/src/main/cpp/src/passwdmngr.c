#include "crypto.h"
#include "storage.h"
#include "util.h"
#include "passwdmngr.h"
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include <sys/stat.h>

#define GET_STR(jstring) (*env)->GetStringUTFChars(env, jstring, 0)
#define FREE_STR(jstring, string) (*env)->ReleaseStringUTFChars(env, jstring, string)

char *err = NULL;

JNIEXPORT jstring JNICALL
Java_com_samuelf09_passwdmngr_Native_getError(JNIEnv *env, jobject thiz)
{
    return (*env)->NewStringUTF(env, err ? err : "No error to report");
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_appInit(JNIEnv *env, jobject thiz, jstring jdata_dir)
{
    char *data_dir = (char *)GET_STR(jdata_dir);
    app_dir = ec_malloc(strlen(data_dir) + 2);
    sprintf(app_dir, "%s/", data_dir);
    FREE_STR(jdata_dir, data_dir);
    if (!app_dir) {
        NATIVE_ERROR("Failed to derive app_dir!");
        return false;
    }

    if (sodium_init() < 0) {
        NATIVE_ERROR("sodium_init failed!");
        return false;
    }

    char *vaults_path = ec_malloc(strlen(app_dir) + strlen("/vaults") + 1);
    sprintf(vaults_path, "%s/vaults", app_dir);
    if (!dir_exists(vaults_path) && mkdir(vaults_path, 0700)) {
        free(vaults_path);
        NATIVE_ERROR("Failed to make app_dir/vaults dir!");
        return false;
    }
    free(vaults_path);

    char *pref_path = ec_malloc(strlen(app_dir) + strlen("/preferences.json") + 1);
    char *accounts_path = ec_malloc(strlen(app_dir) + strlen("/accounts.bin") + 1);
    char *log_path = ec_malloc(strlen(app_dir) + strlen("/passwdmngr.log") + 1);
    sprintf(pref_path, "%s/preferences.json", app_dir);
    sprintf(accounts_path, "%s/accounts.bin", app_dir);
    sprintf(log_path, "%s/passwdmngr.log", app_dir);
    FILE *fp;
    fp = fopen(pref_path, "r");
    if (!fp)
        init_pref_json(pref_path);
    else
        fclose(fp);
    fp = fopen(accounts_path, "r");
    if (!fp) {
        if (!init_accounts()) {
            NATIVE_ERROR("init_accounts() failed!");
            return false;
        }
    } else fclose(fp);
    fp = fopen(log_path, "a");
    if (fp) fclose(fp);

    if (!load_accounts()) {
        NATIVE_ERROR("Failed to load accounts from accounts.bin!");
        return false;
    }

    util_log(LOG_INFO, "App init successful");
    NATIVE_ERROR(NULL);

    return true;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_login(JNIEnv *env, jobject thiz,
                                           jstring jusername, jstring jpassword)
{
    char *uname = (char *)GET_STR(jusername);
    char *password = (char *)GET_STR(jpassword);

    if (!verify_account(uname, password)) {
        NATIVE_ERROR("Invalid username/password");
        return false;
    }

    username = strdup(uname);
    tmp_passwd = strdup(password);
    FREE_STR(jusername, uname);
    FREE_STR(jpassword, password);

    if (!storage_read_user_vault()) {
        NATIVE_ERROR("Failed to read user vault!");
        return false;
    }

    return true;
}

JNIEXPORT void JNICALL
Java_com_samuelf09_passwdmngr_Native_logout(JNIEnv *env, jobject thiz, jboolean shutdown)
{
    if (entries)
        wipe_passwd_entries(entries, num_entries);
    entries     = NULL;
    num_entries = 0;
    if (tmp_passwd) {
        wipe_mem(tmp_passwd, strlen(tmp_passwd));
        free(tmp_passwd);
        tmp_passwd = NULL;
    }
    wipe_mem(aes_key, sizeof(aes_key));
    key_set = false;
    if (username) {
        free(username);
        username = NULL;
    }

    if (shutdown && accounts)
        free(accounts);
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_export(JNIEnv *env, jobject thiz,
                                            jobjectArray jentries, jstring jpath)
{
    char *path = (char *)GET_STR(jpath);

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

    uint8_t *salt = ec_malloc(SALT_LEN);
    randombytes_buf(salt, SALT_LEN);

    bool res = storage_write_vault(path, write_entries, num_write_entries, salt);

    wipe_passwd_entries(write_entries, num_write_entries);
    FREE_STR(jpath, path);
    free(salt);

    return res;
}

JNIEXPORT jintArray JNICALL
Java_com_samuelf09_passwdmngr_Native_getImportConflicts(
        JNIEnv *env, jclass clazz,
        jstring jpath)
{
    // TODO: return int array of conflicts
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_import(
        JNIEnv *env, jclass clazz,
        jstring jpath,
        jstring jpassword,
        jint mode)
{
    char *path = (char *)GET_STR(jpath);

    PasswdEntry *out_entries = NULL;
    VaultHeader *out_hdr = NULL;

    // We use key = NULL first because we need to fill out the
    // header to get the salt, even if a password is provided
    int ret = storage_read_vault_with_key(path, NULL, &out_entries, &out_hdr);
    FREE_STR(jpath, path);

    if (ret == -7 && jpassword) {
        const char *password = GET_STR(jpassword);
        uint8_t key[32];
        if (!derive_vault_key(password, out_hdr->salt, key, sizeof(key))) {
            NATIVE_ERROR("Failed to derive vault key from provided password!");
            FREE_STR(jpassword, password);
            if (out_entries) free(out_entries);
            free(out_hdr);
            return false;
        }
        ret = storage_read_vault_with_key(path, key, &out_entries, &out_hdr);
        FREE_STR(jpassword, password);
    }

    if (ret < 0) {
        util_log(LOG_ERROR, "storage_read_vault_with_key failed with code %d!", ret);
        NATIVE_ERROR("Failed to read data from imported vault!");
        return false;
    }

    // TODO: run import

    wipe_passwd_entries(out_entries, (int)out_hdr->num_entries);
    free(out_hdr);

    return true;
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
Java_com_samuelf09_passwdmngr_Native_getPrefs(
    JNIEnv *env, jclass clazz)
{
    UserPref *pref = get_user_prefs(username);

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
Java_com_samuelf09_passwdmngr_Native_savePrefs(JNIEnv *env, jobject thiz, jobject jprefs)
{
// TODO: implement savePrefs()
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
Java_com_samuelf09_passwdmngr_Native_deleteAccount(
    JNIEnv *env, jclass clazz, jstring juname)
{

    char *uname = (char *)GET_STR(juname);

    bool result = storage_delete_account(uname);

    FREE_STR(juname, uname);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_samuelf09_passwdmngr_Native_changePasswd(
    JNIEnv *env, jclass clazz, jstring juname, jstring jnewPass)
{

    char *uname = (char *)GET_STR(juname);
    char *new_pass = (char *)GET_STR(jnewPass);

    bool result = storage_change_passwd(uname, new_pass);

    FREE_STR(juname, uname);
    FREE_STR(jnewPass, new_pass);

    return result;
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

JNIEXPORT jobjectArray JNICALL
Java_com_samuelf09_passwdmngr_Native_storageGetEntries(JNIEnv *env, jobject thiz)
{
    if (num_entries < 0) return NULL;

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

    jobjectArray arr = (*env)->NewObjectArray(env, num_entries, entryCls, NULL);
    if (!arr)
    {
        util_log(LOG_ERROR, "Failed to allocate PasswdEntry array!");
        return NULL;
    }

    for (int i = 0; i < num_entries; i++)
    {
        int e_id = entries[i].id;
        char *e_service = entries[i].service;
        char *e_username = entries[i].username;
        char *e_password = entries[i].password;
        char *e_notes = entries[i].notes;

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

    return arr;
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