package com.samuelf09.passwdmngr

data class EncryptionResult(val ciphertext: ByteArray, val tag: ByteArray)
data class EncryptEntriesResult(val ct: ByteArray, val tag: ByteArray, val nonce: ByteArray)

data class AccountHeader(
    val magic: ByteArray,
    val version: Int,
    val hash: ByteArray,
    val numAccounts: Int
)

data class Account(val unameHash: ByteArray, val passwdHash: ByteArray)

data class UserPref(val unameHash: String?)

data class PasswdEntry(
    val id: Int,
    val service: String?,
    val username: String?,
    val password: String?,
    val notes: String?
)

data class VaultHeader(
    val magic: ByteArray,
    val version: Int,
    val hash: ByteArray,
    val timestamp: Long,
    val numEntries: Int,
    val ciphertextLen: Int,
    val salt: ByteArray,
    val nonce: ByteArray,
    val tag: ByteArray
)

data class VaultReadResult(val entries: Array<PasswdEntry>, val hdr: VaultHeader)

object Native {
    init {
        System.loadLibrary("passwdmngr-core")
    }

    external fun appInit(dataDir: String): Boolean

    // CRYPTO.H
    external fun verifyAccount(uname: String, passwd: String): Boolean

    external fun verifyPwHash(hash: ByteArray, passwd: String): Boolean
    external fun hashPwWithSalt(passwd: String, salt: ByteArray): ByteArray
    external fun hashPw(passwd: String): ByteArray
    external fun hashUname(uname: String): String?

    external fun sha256Hash(data: ByteArray): ByteArray

    external fun deriveVaultKey(passwd: String, salt: ByteArray): ByteArray
    external fun genPasswd(
        len: Int,
        special: String,
        digits: Boolean,
        uppers: Boolean,
        lowers: Boolean
    ): String?

    external fun aesGcmEncrypt(
        plaintext: ByteArray,
        key: ByteArray,
        iv: ByteArray
    ): EncryptionResult?

    external fun aesGcmDecrypt(
        ciphertext: ByteArray,
        key: ByteArray,
        iv: ByteArray,
        tag: ByteArray
    ): ByteArray

    // STORAGE.H

    external fun loadAccounts(): Boolean
    external fun initAccounts(): Boolean
    external fun initPrefJson(prefPath: String)
    external fun saveAccounts()

    external fun storageReadPrefs(): Array<UserPref>?
    external fun storageSavePrefs(prefs: Array<UserPref>): Boolean
    external fun getUserPrefs(username: String): UserPref?

    external fun createNewAccount(uname: String, password: String): Boolean
    external fun storageDeleteAccount(uname: String): Boolean
    external fun storageChangePasswd(uname: String, newPass: String): Boolean

    external fun getUserSalt(): ByteArray?
    external fun storageGetUserVaultPath(uname: String): String?

    external fun storageDumpJson(
        vaultPath: String,
        key: ByteArray,
        pretty: Boolean
    ): String?

    external fun encryptEntries(
        entries: Array<PasswdEntry>,
        salt: ByteArray
    ): EncryptEntriesResult?

    external fun decryptEntriesWithKey(
        key: ByteArray,
        ciphertext: ByteArray,
        nonce: ByteArray,
        tag: ByteArray
    ): Array<PasswdEntry>?

    external fun decryptEntries(
        salt: ByteArray,
        ciphertext: ByteArray,
        nonce: ByteArray,
        tag: ByteArray
    ): Array<PasswdEntry>?

    external fun storageReadVaultWithKey(
        vaultPath: String,
        key: ByteArray
    ): VaultReadResult?

    external fun storageReadVault(
        vaultPath: String
    ): VaultReadResult?

    external fun storageReadUserVault(): Boolean

    external fun storageWriteVault(
        vaultPath: String,
        entries: Array<PasswdEntry>,
        salt: ByteArray
    ): Boolean

    external fun storageWriteUserVault(): Boolean

    external fun storageGetNextId(): Int
    external fun storageGetEntry(id: Int): PasswdEntry?

    external fun addEntry(entry: PasswdEntry): Boolean
    external fun deleteEntry(id: Int): Boolean
    external fun updateEntry(id: Int, newEntry: PasswdEntry): Boolean

    external fun wipePasswdEntries(entries: Array<PasswdEntry>)

    // UTIL.H

    external fun utilGetLogfile(): String?
    external fun utilGetPrefsFile(): String?
    external fun utilGetAccountsFile(): String?

    external fun utilAssert(cond: Int, failMsg: String)
    external fun utilLog(level: Int, message: String)

    external fun wipeMem(bytes: ByteArray)
    external fun countSubstrings(haystack: String, needle: String): Int
}