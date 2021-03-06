/*
 * File:   LuaBinding.cpp
 * Author: WU Jun <quark@lihdd.net>
 */

#include "LuaBinding.h"
#include "Configuration.h"
#include "defines.h"
#include "XUtility.h"
#include "engine.h"

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <ibus.h>

// Lua C functions

#define IME_LIBRARY_NAME "ime"

int LuaBinding::l_executeScript(lua_State * L) {
    DEBUG_PRINT(2, "[LUABIND] l_executeScript\n");
    luaL_checktype(L, 1, LUA_TSTRING);
    LuaBinding& lb = *luaStates[L];
    int r = lb.doString(lua_tostring(L, 1));
    lua_pushboolean(L, r == 0);
    return 1;
}

int LuaBinding::l_printStack(lua_State *L) {
    DEBUG_PRINT(3, "[LUABIND] l_printStack\n");
    int c;
    printf("LuaStack element count: %d\n", c = lua_gettop(L));
    for (int i = 1; i <= c; i++) {
        printf("  Emement[-%2d]: ", i);
        switch (lua_type(L, -i)) {
            case LUA_TNUMBER:
                printf("%.2f\n", lua_tonumber(L, -i));
                break;
            case LUA_TBOOLEAN:
                printf("%s\n", lua_toboolean(L, -i) ? "true" : "false");
                break;
            case LUA_TSTRING:
                printf("%s\n", lua_tostring(L, -i));
                break;
            case LUA_TNIL:
                printf("nil\n");
                break;
            case LUA_TTABLE:
                printf("table\n");
                break;
            default:
                printf("(%s)\n", lua_typename(L, lua_type(L, -i)));
        }
        fflush(stdout);
    }
    return 0;
}

int LuaBinding::l_bitand(lua_State *L) {
    DEBUG_PRINT(3, "[LUABIND] l_bitand\n");
    LuaBinding* lib = luaStates[L];
    pthread_mutex_lock(&lib->luaStateAtomMutex);
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_checktype(L, 2, LUA_TNUMBER);
    lua_checkstack(L, 1);
    lua_pushinteger(L, lua_tointeger(L, 1) & lua_tointeger(L, 2));
    pthread_mutex_unlock(&lib->luaStateAtomMutex);

    return 1;
}

int LuaBinding::l_bitxor(lua_State *L) {
    DEBUG_PRINT(3, "[LUA] l_bitxor\n");
    LuaBinding* lib = luaStates[L];
    pthread_mutex_lock(&lib->luaStateAtomMutex);
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_checktype(L, 2, LUA_TNUMBER);
    lua_checkstack(L, 1);
    lua_pushinteger(L, lua_tointeger(L, 1) ^ lua_tointeger(L, 2));
    pthread_mutex_unlock(&lib->luaStateAtomMutex);
    return 1;
}

int LuaBinding::l_bitor(lua_State *L) {
    DEBUG_PRINT(2, "[LUA] l_bitor\n");
    LuaBinding* lib = luaStates[L];
    pthread_mutex_lock(&lib->luaStateAtomMutex);
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_checktype(L, 2, LUA_TNUMBER);
    lua_checkstack(L, 1);
    lua_pushinteger(L, lua_tointeger(L, 1) | lua_tointeger(L, 2));
    pthread_mutex_unlock(&lib->luaStateAtomMutex);
    return 1;
}

const struct luaL_Reg LuaBinding::luaLibraryReg[] = {
    //{"set_double_pinyin_scheme", LuaBinding::l_setDoublePinyinScheme},
    //{"double_to_full_pinyin", LuaBinding::l_doubleToFullPinyin},
    //{"is_valid_double_pinyin", LuaBinding::l_isValidDoublePinyin},
    {"bitand", LuaBinding::l_bitand},
    //{"bitxor", LuaBinding::l_bitxor},
    {"bitor", LuaBinding::l_bitor},
    //{"keymask", LuaBinding::l_keymask},
    //{"printStack", LuaBinding::l_printStack},
    //{"chars_to_pinyin", LuaBinding::l_charsToPinyin},
    //{"set_debug_level", LuaBinding::l_setDebugLevel},
    //{"load_database", LuaBinding::l_loadPhraseDatabase},
    //{"database_count", LuaBinding::l_getPhraseDatabaseLoadedCount},
    //{"apply_settings", LuaBinding::l_applySettings},
    //{"get_selection", LuaBinding::l_getSelection},
    //{"notify", LuaBinding::l_notify},
    {"execute", LuaBinding::l_executeScript},
    //{"commit", Engine::l_commitText},
    //{"request", Engine::l_sendRequest},
    //{"register_command", Configuration::l_registerCommand},
    {NULL, NULL}
};

// LuaBinding class

map<const lua_State*, LuaBinding*> LuaBinding::luaStates;

const char LuaBinding::LIB_NAME[] = IME_LIBRARY_NAME;
const string LuaBinding::LibraryName = LuaBinding::LIB_NAME;



LuaBinding::LuaBinding() {
    DEBUG_PRINT(1, "[LUABIND] Init\n");

    // init mutex
    pthread_mutexattr_t luaStateMutexAttribute;
    pthread_mutexattr_init(&luaStateMutexAttribute);
    pthread_mutexattr_settype(&luaStateMutexAttribute, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&luaStateAtomMutex, &luaStateMutexAttribute)
            || pthread_mutex_init(&luaStateFunctionMutex, &luaStateMutexAttribute)) {
        fprintf(stderr, "fail to create lua state mutex.\nAborted.\n");
        exit(EXIT_FAILURE);
    }
    pthread_mutexattr_destroy(&luaStateMutexAttribute);

    // init lua state
    L = lua_open();
    luaL_openlibs(L);

    // add custom library functions. after this, stack has 1 element.
    luaL_register(L, LIB_NAME, luaLibraryReg);
    lua_pop(L, 1);

    // set constants
    setValue("VERSION", VERSION);
    setValue("PKGDATADIR", PKGDATADIR);
    setValue("USERCONFIGDIR", ((string) (g_get_user_config_dir()) + G_DIR_SEPARATOR_S "ibus" G_DIR_SEPARATOR_S "sogoupycc").c_str());
    setValue("USERCACHEDIR", ((string) (g_get_user_cache_dir()) + G_DIR_SEPARATOR_S "ibus" G_DIR_SEPARATOR_S "sogoupycc").c_str());
    setValue("USERDATADIR", ((string) (g_get_user_data_dir()) + G_DIR_SEPARATOR_S "ibus" G_DIR_SEPARATOR_S "sogoupycc").c_str());
    setValue("DIRSEPARATOR", G_DIR_SEPARATOR_S);

    Configuration::addConstantsToLua(*this);

    // insert into static map, let lib function be able to lookup this class
    // L - this one-to-one relation. (Now it takes O(logN) to lookup 'this'
    // from L. Any way to make it more smooth?)
    luaStates[L] = this;
}

void LuaBinding::staticInit() {
    // load config, modify static vars:
    // doublePinyinScheme, pinyinDatabases, etc
    staticLuaBinding = new LuaBinding();
}

void LuaBinding::loadStaticConfigure() {
    staticLuaBinding->doString("dofile('" PKGDATADIR G_DIR_SEPARATOR_S "config.lua') " IME_LIBRARY_NAME ".apply_settings()");
}

void LuaBinding::staticDestruct() {
    DEBUG_PRINT(1, "[LUABIND] Static Destroy\n");
    delete staticLuaBinding;
}

const lua_State* LuaBinding::getLuaState() const {
    return L;
}

/**
 * return 0 if no error
 */
int LuaBinding::doString(const char* luaScript, bool locked) {
    DEBUG_PRINT(3, "[LUABIND] doString(%s)\n", luaScript);
    if (locked) pthread_mutex_lock(&luaStateFunctionMutex);
    int r = (luaL_loadstring(L, luaScript) || lua_pcall(L, 0, 0, 0));
    if (r) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        if (Configuration::showNotification) {
            XUtility::showNotify("Lua 错误", lua_tostring(L, -1), "error");
        }
        lua_pop(L, 1);
    }
    if (locked) pthread_mutex_unlock(&luaStateFunctionMutex);
    return r;
}

int LuaBinding::callLuaFunction(const char * const funcName, const char* sig, ...) {
    DEBUG_PRINT(2, "[LUABIND] callLua: %s (%s)\n", funcName, sig);

    int narg, nres, ret = 0;

    va_list vl;
    va_start(vl, sig);

    //assert(lua_gettop(L) == 0);
    lua_State * state = L;

    pthread_mutex_lock(&luaStateFunctionMutex);
    lua_checkstack(state, strlen(sig) + 2);
    int pushedCount = reachValue(funcName, "_G");

    if (lua_isnil(state, -1)) {
        // function not found, here just ignore and skip.
        ret = -1;
        goto aborting;
    }

    for (narg = 0; *sig; ++narg, ++sig) {
        luaL_checkstack(state, 1, "too many arguments");
        if (*sig == 'd') lua_pushinteger(state, va_arg(vl, int));
        else if (*sig == 'f') lua_pushnumber(state, va_arg(vl, double));
        else if (*sig == 's') lua_pushstring(state, va_arg(vl, char*));
        else if (*sig == 'b') lua_pushboolean(state, va_arg(vl, int));
        else if (*sig == '>' || *sig == '|' || *sig == ' ' || *sig == '=') {
            sig++;
            break;
        } else {
            fprintf(stderr, "invalid option (%c) in callLua\n", *sig);
        }
    }

    // lua_pcall will remove paras and function
    pushedCount--;
    nres = strlen(sig);
    // since we use recursive mutex here, no unlock needed
    DEBUG_PRINT(3, "[LUABIND.CALLLUA] Params pushed\n");
    if ((ret = lua_pcall(state, narg, nres, 0)) != 0) {
        fprintf(stderr, "error calling lua function '%s.%s': %s.\nprogram is likely to crash soon\n", LIB_NAME, funcName, lua_tostring(state, -1));
        if (Configuration::showNotification) {
            XUtility::showNotify("Lua 错误", lua_tostring(state, -1), "error");
        }
        pushedCount++;
        // report error early.
        goto aborting;
    }
    DEBUG_PRINT(3, "[LUABIND.CALLLUA] Pcall finished\n");

    for (int pres = -nres; *sig; ++pres, ++sig) {
        switch (*sig) {
            case 'd':
                if (!lua_isnumber(state, pres)) fprintf(stderr, "Lua function '%s.%s' result type error. expect integer.\n", LIB_NAME, funcName);
                else *va_arg(vl, int*) = lua_tointeger(state, pres);
                break;
            case 'f':
                if (!lua_isnumber(state, pres)) fprintf(stderr, "Lua function '%s.%s' result type error. expect number.\n", LIB_NAME, funcName);
                else *va_arg(vl, double*) = lua_tonumber(state, pres);
                break;
            case 's':
                if (!lua_isstring(state, pres)) fprintf(stderr, "Lua function '%s.%s' result type error. expect string.\n", LIB_NAME, funcName);
                else *va_arg(vl, string*) = string(lua_tostring(state, pres));
                break;
            case 'b':
                if (!lua_isboolean(state, pres)) fprintf(stderr, "Lua function '%s.%s' result type error. expect boolean.\n", LIB_NAME, funcName);
                else *va_arg(vl, int*) = lua_toboolean(state, pres);
                break;
            default:
                fprintf(stderr, "invalid option (%c) in callLua\n", *sig);
        }

    }
    DEBUG_PRINT(3, "[LUABIND.CALLLUA] Cleaning\n");
    DEBUG_PRINT(3, "[LUABIND.CALLLUA] Stack size: %d, result count: %d\n", lua_gettop(state), nres);

aborting:
    // pop values before enter here
    lua_pop(state, pushedCount);
    pthread_mutex_unlock(&luaStateFunctionMutex);
    va_end(vl);

    //assert(lua_gettop(L) == 0);
    return ret;
}

void LuaBinding::registerFunction(const lua_CFunction func, const char * funcName) {
    DEBUG_PRINT(1, "[LUABIND] addFunction: %s\n", funcName);

    pthread_mutex_lock(&luaStateAtomMutex);
    lua_checkstack(L, 3);
    lua_getglobal(L, LIB_NAME); // should be a table
    lua_pushstring(L, funcName); // key
    lua_pushcfunction(L, func); // value
    lua_settable(L, -3); // will pop key and value
    lua_pop(L, 1); // stack should be empty.
    pthread_mutex_unlock(&luaStateAtomMutex);
}

static vector<string> splitString(string toBeSplited, char separator) {
    vector<string> r;
    size_t lastPos = 0;
    for (size_t pos = toBeSplited.find(separator);; pos = toBeSplited.find(separator, pos + 1)) {
        r.push_back(toBeSplited.substr(lastPos, pos - lastPos));
        if (pos == string::npos) break;
        lastPos = pos + 1;
    }
    return r;
}

int LuaBinding::reachValue(const char* varName, const char* libName) {
    DEBUG_PRINT(5, "[LUABIND] reachValue: %s.%s\n", libName, varName);
    // internal use, lock before call this
    vector<string> names = splitString(varName, '.');

    int pushedCount = 1;
    lua_checkstack(L, 1 + names.size());
    lua_getglobal(L, libName);

    bool nextIsNumber = false;
    for (size_t i = 0; i < names.size(); i++) {
        string& name = names[i];

        if (name.empty()) {
            nextIsNumber = true;
            continue;
        }
        if (!lua_istable(L, -1)) {
            DEBUG_PRINT(6, "[LUABIND] Not a table, stop\n");
            // stop here
            break;
        }
        if (nextIsNumber) {
            int id;
            DEBUG_PRINT(6, "[LUABIND] number: %s\n", name.c_str());
            sscanf(name.c_str(), "%d", &id);
            lua_pushnumber(L, id);
            nextIsNumber = false;
        } else {
            DEBUG_PRINT(6, "[LUABIND] string: %s\n", name.c_str());
            lua_pushstring(L, name.c_str());
        }
        pushedCount++;
        lua_gettable(L, -2);
    }

    return pushedCount;
}

bool LuaBinding::getValue(const char* varName, const bool defaultValue, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] getValue(boolean): %s.%s\n", libName, varName);
    pthread_mutex_lock(&luaStateAtomMutex);
    bool r = defaultValue;
    int pushedCount = reachValue(varName, libName);
    if (lua_isboolean(L, -1)) r = lua_toboolean(L, -1);
    lua_pop(L, pushedCount);
    pthread_mutex_unlock(&luaStateAtomMutex);
    return r;
}

string LuaBinding::getValue(const char* varName, const char* defaultValue, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] getValue(string): %s.%s\n", libName, varName);
    pthread_mutex_lock(&luaStateAtomMutex);
    string r = defaultValue;
    int pushedCount = reachValue(varName, libName);
    if (lua_isstring(L, -1)) r = lua_tostring(L, -1);
    lua_pop(L, pushedCount);
    pthread_mutex_unlock(&luaStateAtomMutex);
    return r;
}

int LuaBinding::getValue(const char* varName, const int defaultValue, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] getValue(int): %s.%s\n", libName, varName);
    pthread_mutex_lock(&luaStateAtomMutex);
    int r = defaultValue;
    int pushedCount = reachValue(varName, libName);
    if (lua_isnumber(L, -1)) r = lua_tointeger(L, -1);
    lua_pop(L, pushedCount);
    pthread_mutex_unlock(&luaStateAtomMutex);
    return r;
}

size_t LuaBinding::getValue(const char* varName, const size_t defaultValue, const char* libName) {
    DEBUG_PRINT(5, "[LUABIND] getValue(size_t): %s.%s\n", libName, varName);
    return (size_t) getValue(varName, (int) defaultValue, libName);
}

double LuaBinding::getValue(const char* varName, const double defaultValue, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] getValue(double): %s.%s\n", libName, varName);
    pthread_mutex_lock(&luaStateAtomMutex);
    double r = defaultValue;
    int pushedCount = reachValue(varName, libName);
    if (lua_isnumber(L, -1)) r = lua_tonumber(L, -1);
    lua_pop(L, pushedCount);
    pthread_mutex_unlock(&luaStateAtomMutex);
    return r;
}

int LuaBinding::getValueType(const char* varName, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] getValueType: %s.%s\n", libName, varName);
    pthread_mutex_lock(&luaStateAtomMutex);
    int pushedCount = reachValue(varName, libName);
    int r = lua_type(L, -1);
    lua_pop(L, pushedCount);
    pthread_mutex_unlock(&luaStateAtomMutex);
    DEBUG_PRINT(6, "[LUABIND] getValueType: return %s\n", r == LUA_TSTRING ? "STRING" : (r == LUA_TNIL ? "NIL" : (r == LUA_TNUMBER ? "NUMBER" : (r == LUA_TTABLE ? "TABLE" : "<OTHER>"))));
    return r;
}

void LuaBinding::setValue(const char* varName, const int value, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] setValue(int): %s.%s = %d\n", libName, varName, value);
    pthread_mutex_lock(&luaStateAtomMutex);
    lua_checkstack(L, 2);
    lua_getglobal(L, libName);
    if (lua_istable(L, -1)) {

        lua_pushinteger(L, value);
        lua_setfield(L, -2, varName); //it will pop value
    }
    lua_pop(L, 1);
    pthread_mutex_unlock(&luaStateAtomMutex);
}

void LuaBinding::setValue(const char* varName, const char value[], const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] setValue(string): %s.%s = '%s'\n", libName, varName, value);
    pthread_mutex_lock(&luaStateAtomMutex);
    lua_checkstack(L, 2);
    lua_getglobal(L, libName);
    if (lua_istable(L, -1)) {

        lua_pushstring(L, value);
        lua_setfield(L, -2, varName); //it will pop value
    }
    lua_pop(L, 1);
    pthread_mutex_unlock(&luaStateAtomMutex);
}

void LuaBinding::setValue(const char* varName, const bool value, const char* libName) {
    DEBUG_PRINT(4, "[LUABIND] setValue(boolean): %s.%s = %s\n", libName, varName, value ? "true" : "false");
    pthread_mutex_lock(&luaStateAtomMutex);
    lua_checkstack(L, 2);
    lua_getglobal(L, libName);
    if (lua_istable(L, -1)) {

        lua_pushboolean(L, value);
        lua_setfield(L, -2, varName); //it will pop value
    }
    lua_pop(L, 1);
    pthread_mutex_unlock(&luaStateAtomMutex);
}

/**
 * this is private and should not be used.
 */
LuaBinding::LuaBinding(const LuaBinding& orig) {

}

LuaBinding::~LuaBinding() {
    DEBUG_PRINT(1, "[LUABIND] Destroy\n");
    luaStates.erase(L);
    lua_close(L);
    pthread_mutex_destroy(&luaStateAtomMutex);
    pthread_mutex_destroy(&luaStateFunctionMutex);
}

LuaBinding* LuaBinding::staticLuaBinding = NULL;

LuaBinding& LuaBinding::getStaticBinding() {
    return *staticLuaBinding;
}

LuaBinding& LuaBinding::getLuaBinding(lua_State* L) {
    if (luaStates.find(L) == luaStates.end()) return getStaticBinding();
    return *luaStates.find(L)->second;
}

pthread_mutex_t* LuaBinding::getAtomMutex() {
    return &luaStateAtomMutex;
}

