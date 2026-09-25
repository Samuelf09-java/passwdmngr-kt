package com.samuelf09.passwdmngr

data class UserPref(val unameHash: String?)

data class PasswdEntry(
    val id: Int,
    val service: String?,
    val username: String?,
    val password: String?,
    val notes: String?
)

fun emptyPasswdEntry() = PasswdEntry(id = -1, service = "", username = "", password = "", notes = "")

object Native {
    init {
        System.loadLibrary("passwdmngr-core")
    }

    external fun getError(): String

    external fun appInit(dataDir: String): Boolean
    external fun login(username: String, password: String): Boolean
    external fun logout(shutdown: Boolean = false)
    external fun getPrefs(): UserPref?
    external fun savePrefs(prefs: UserPref): Boolean
    external fun createNewAccount(uname: String, password: String): Boolean
    external fun deleteAccount(uname: String): Boolean
    external fun changePasswd(uname: String, newPass: String): Boolean
    external fun export(entries: Array<PasswdEntry>, path: String): Boolean
    external fun getImportConflicts(path: String): Array<Int>
    external fun import(path: String, password: String?, mode: Int): Boolean
    external fun addEntry(entry: PasswdEntry): Boolean

    external fun deleteEntry(id: Int): Boolean
    external fun updateEntry(id: Int, newEntry: PasswdEntry): Boolean
    external fun storageGetNextId(): Int

    external fun storageGetEntry(id: Int): PasswdEntry?

    external fun storageGetEntries(): Array<PasswdEntry>?
    external fun genPasswd(
        len: Int,
        special: String,
        digits: Boolean,
        uppers: Boolean,
        lowers: Boolean
    ): String?
}