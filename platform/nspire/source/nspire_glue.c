// nspire_glue.c
//
// C-only wrapper around the Ndless SDK. It exists because the Ndless headers
// (<libndls.h>, <dirent.h>, <syscall-decls.h>) declare a stub Lua API
// (lua_pcall, luaL_checknumber, ...) whose declarations clash with z8lua's
// own Lua headers when both are seen by the same C++ translation unit:
//
//   - syscall-decls.h declares lua_resume(lua_State*, int) etc. while
//     z8lua's lua.h declares lua_resume(lua_State*, lua_State*, int).
//   - z8lua/lauxlib.h declares functions whose C++ signatures (lua_Number =
//     z8::fix32) conflict with the SDK's `extern "C"` declarations of the
//     same names.
//
// Keeping all Ndless SDK includes in this C-only file fully decouples those
// two worlds. NspireHost.cpp then talks to the SDK only through the small
// set of plain C functions exported below.

#include <libndls.h>
#include <keys.h>
#include <dirent.h>
#include <sys/stat.h>

// --- Input ----------------------------------------------------------------

int nspire_key_up(void)    { return isKeyPressed(KEY_NSPIRE_8) || isKeyPressed(KEY_NSPIRE_UP) || isKeyPressed(KEY_NSPIRE_9) || isKeyPressed(KEY_NSPIRE_7); }
int nspire_key_down(void)  { return isKeyPressed(KEY_NSPIRE_5) || isKeyPressed(KEY_NSPIRE_DOWN) || isKeyPressed(KEY_NSPIRE_1) || isKeyPressed(KEY_NSPIRE_3) || isKeyPressed(KEY_NSPIRE_2); }
int nspire_key_left(void)  { return isKeyPressed(KEY_NSPIRE_4) || isKeyPressed(KEY_NSPIRE_LEFT) || isKeyPressed(KEY_NSPIRE_7) || isKeyPressed(KEY_NSPIRE_1); }
int nspire_key_right(void) { return isKeyPressed(KEY_NSPIRE_6) || isKeyPressed(KEY_NSPIRE_RIGHT) || isKeyPressed(KEY_NSPIRE_9) || isKeyPressed(KEY_NSPIRE_6); }
int nspire_key_x(void)     { return isKeyPressed(KEY_NSPIRE_VAR); }
int nspire_key_o(void)     { return isKeyPressed(KEY_NSPIRE_DEL); }
int nspire_key_pause(void) { return isKeyPressed(KEY_NSPIRE_ENTER); }
int nspire_key_tab(void)   { return isKeyPressed(KEY_NSPIRE_TAB); }
int nspire_key_esc(void)   { return isKeyPressed(KEY_NSPIRE_ESC); }

// --- Filesystem -----------------------------------------------------------
//
// The Ndless SDK aliases `DIR`/`opendir`/`readdir`/`closedir` to its own
// NUC_* equivalents via inline wrappers in <dirent.h>. The struct dirent
// here is actually `nuc_dirent` (one char d_name[1] flexible member).

typedef void (*nspire_dir_cb)(const char *name, void *user);

int nspire_list_dir(const char *path, nspire_dir_cb cb, void *user) {
    DIR *dir = opendir(path);
    if (!dir) return -1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        cb(ent->d_name, user);
    }
    closedir(dir);
    return 0;
}

int nspire_mkdir(const char *path) {
    return mkdir(path, 0777);
}

int nspire_path_is_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return 0;
    closedir(dir);
    return 1;
}
