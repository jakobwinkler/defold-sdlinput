#include <dmsdk/sdk.h>
#include <string.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_joystick.h>

#define MODULE_NAME "sdlinput"

struct Constant {
    const char* name;
    int value;
};

// Constants arrays — single source of truth for registration and iteration
static const Constant BUTTON_CONSTS[] = {
    {"BUTTON_SOUTH",              SDL_GAMEPAD_BUTTON_SOUTH},
    {"BUTTON_EAST",               SDL_GAMEPAD_BUTTON_EAST},
    {"BUTTON_WEST",               SDL_GAMEPAD_BUTTON_WEST},
    {"BUTTON_NORTH",              SDL_GAMEPAD_BUTTON_NORTH},
    {"BUTTON_BACK",               SDL_GAMEPAD_BUTTON_BACK},
    {"BUTTON_GUIDE",              SDL_GAMEPAD_BUTTON_GUIDE},
    {"BUTTON_START",              SDL_GAMEPAD_BUTTON_START},
    {"BUTTON_LEFT_STICK",         SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"BUTTON_RIGHT_STICK",        SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"BUTTON_LEFT_SHOULDER",      SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"BUTTON_RIGHT_SHOULDER",     SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"BUTTON_DPAD_UP",            SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"BUTTON_DPAD_DOWN",          SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"BUTTON_DPAD_LEFT",          SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"BUTTON_DPAD_RIGHT",         SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"BUTTON_MISC1",              SDL_GAMEPAD_BUTTON_MISC1},
    {"BUTTON_MISC2",              SDL_GAMEPAD_BUTTON_MISC2},
    {"BUTTON_MISC3",              SDL_GAMEPAD_BUTTON_MISC3},
    {"BUTTON_MISC4",              SDL_GAMEPAD_BUTTON_MISC4},
    {"BUTTON_MISC5",              SDL_GAMEPAD_BUTTON_MISC5},
    {"BUTTON_LEFT_PADDLE1",       SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
    {"BUTTON_LEFT_PADDLE2",       SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
    {"BUTTON_RIGHT_PADDLE1",      SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1},
    {"BUTTON_RIGHT_PADDLE2",      SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2},
    {"BUTTON_TOUCHPAD",           SDL_GAMEPAD_BUTTON_TOUCHPAD},
    {NULL, 0}
};

static const Constant AXIS_CONSTS[] = {
    {"AXIS_LEFTX",                SDL_GAMEPAD_AXIS_LEFTX},
    {"AXIS_LEFTY",                SDL_GAMEPAD_AXIS_LEFTY},
    {"AXIS_RIGHTX",               SDL_GAMEPAD_AXIS_RIGHTX},
    {"AXIS_RIGHTY",               SDL_GAMEPAD_AXIS_RIGHTY},
    {"AXIS_LEFT_TRIGGER",         SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
    {"AXIS_RIGHT_TRIGGER",        SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
    {NULL, 0}
};

static const Constant SENSOR_CONSTS[] = {
    {"SENSOR_GYRO",               SDL_SENSOR_GYRO},
    {"SENSOR_ACCEL",              SDL_SENSOR_ACCEL},
    {NULL, 0}
};

static const Constant TYPE_CONSTS[] = {
    {"GAMEPAD_TYPE_STANDARD",     SDL_GAMEPAD_TYPE_STANDARD},
    {"GAMEPAD_TYPE_XBOX360",      SDL_GAMEPAD_TYPE_XBOX360},
    {"GAMEPAD_TYPE_XBOXONE",      SDL_GAMEPAD_TYPE_XBOXONE},
    {"GAMEPAD_TYPE_PS3",          SDL_GAMEPAD_TYPE_PS3},
    {"GAMEPAD_TYPE_PS4",          SDL_GAMEPAD_TYPE_PS4},
    {"GAMEPAD_TYPE_PS5",          SDL_GAMEPAD_TYPE_PS5},
    {"GAMEPAD_TYPE_NINTENDO_SWITCH_PRO", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO},
    {"GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT",  SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT},
    {"GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT", SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT},
    {"GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR",  SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR},
    {"GAMEPAD_TYPE_GAMECUBE", SDL_GAMEPAD_TYPE_GAMECUBE},
    {NULL, 0}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int lua_fail(lua_State* L, const char* err)
{
    lua_pushnil(L);
    lua_pushstring(L, err ? err : "unknown error");
    return 2;
}

static int lua_fail_sdl(lua_State* L)
{
    return lua_fail(L, SDL_GetError());
}

static int LuaInit(lua_State* L)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int LuaQuit(lua_State* L)
{
    SDL_Quit();
    return 0;
}

static int LuaPumpEvents(lua_State* L)
{
    SDL_PumpEvents();
    return 0;
}

static int LuaNumGamepads(lua_State* L)
{
    SDL_PumpEvents();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) {
        return lua_fail_sdl(L);
    }
    SDL_free(ids);
    lua_pushinteger(L, count);
    return 1;
}

static int LuaGamepadName(lua_State* L)
{
    SDL_PumpEvents();
    int idx = (int)luaL_checkinteger(L, 1);
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return lua_fail_sdl(L);
    if (idx < 0 || idx >= count) {
        SDL_free(ids);
        return lua_fail(L, "index out of range");
    }
    const char* name = SDL_GetGamepadNameForID(ids[idx]);
    SDL_free(ids);
    if (!name) return lua_fail_sdl(L);
    lua_pushstring(L, name);
    return 1;
}

static int LuaGamepadOpen(lua_State* L)
{
    SDL_PumpEvents();
    int idx = (int)luaL_checkinteger(L, 1);
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return lua_fail_sdl(L);
    if (idx < 0 || idx >= count) {
        SDL_free(ids);
        return lua_fail(L, "index out of range");
    }
    SDL_Gamepad* gp = SDL_OpenGamepad(ids[idx]);
    SDL_free(ids);
    if (!gp) return lua_fail_sdl(L);
    lua_pushlightuserdata(L, gp);
    return 1;
}

static int LuaGamepadClose(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (gp) {
        SDL_CloseGamepad(gp);
    }
    return 0;
}

static int LuaGamepadConnected(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    lua_pushboolean(L, gp ? SDL_GamepadConnected(gp) : 0);
    return 1;
}

static int LuaGamepadGetType(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp || !SDL_GamepadConnected(gp)) {
        lua_pushstring(L, "unknown");
        return 1;
    }
    SDL_GamepadType t = SDL_GetGamepadType(gp);
    const char* s = SDL_GetGamepadStringForType(t);
    lua_pushstring(L, s ? s : "unknown");
    return 1;
}

static int LuaGamepadGetVid(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, SDL_GetGamepadVendor(gp));
    return 1;
}

static int LuaGamepadGetPid(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, SDL_GetGamepadProduct(gp));
    return 1;
}

static int LuaGamepadGetButtons(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp || !SDL_GamepadConnected(gp)) {
        lua_pushnil(L);
        return 1;
    }

    SDL_PumpEvents();
    lua_createtable(L, 0, 25);

    for (int i = 0; BUTTON_CONSTS[i].name; ++i) {
        bool pressed = SDL_GetGamepadButton(gp, (SDL_GamepadButton)BUTTON_CONSTS[i].value);
        lua_pushboolean(L, pressed);
        lua_rawseti(L, -2, BUTTON_CONSTS[i].value);
    }
    return 1;
}

static int LuaGamepadGetButton(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp || !SDL_GamepadConnected(gp)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int btn = (int)luaL_checkinteger(L, 2);
    SDL_PumpEvents();
    lua_pushboolean(L, SDL_GetGamepadButton(gp, (SDL_GamepadButton)btn));
    return 1;
}

static int LuaGamepadGetAxis(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    int axis = (int)luaL_checkinteger(L, 2);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushnumber(L, 0); return 1; }

    SDL_PumpEvents();
    Sint16 val = SDL_GetGamepadAxis(gp, (SDL_GamepadAxis)axis);
    float normalized;
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        normalized = val / 32767.0f;
        if (normalized < 0) normalized = 0;
    } else {
        normalized = val / 32767.0f;
        if (normalized < -1.0f) normalized = -1.0f;
    }
    lua_pushnumber(L, normalized);
    return 1;
}

static int LuaGamepadGetSensor(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    int type = (int)luaL_checkinteger(L, 2);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushnil(L); return 1; }

    if (type != SDL_SENSOR_GYRO && type != SDL_SENSOR_ACCEL) {
        return lua_fail(L, "sensor constant must be sdlinput.SENSOR_GYRO or sdlinput.SENSOR_ACCEL");
    }

    SDL_PumpEvents();
    float data[3] = {0};
    if (!SDL_GetGamepadSensorData(gp, (SDL_SensorType)type, data, 3)) {
        return lua_fail_sdl(L);
    }
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, data[0]); lua_setfield(L, -2, "x");
    lua_pushnumber(L, data[1]); lua_setfield(L, -2, "y");
    lua_pushnumber(L, data[2]); lua_setfield(L, -2, "z");
    return 1;
}

static int LuaGamepadSetSensorEnabled(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    int type = (int)luaL_checkinteger(L, 2);
    bool enabled = lua_toboolean(L, 3) != 0;
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushboolean(L, 0); return 1; }

    if (type != SDL_SENSOR_GYRO && type != SDL_SENSOR_ACCEL) {
        return lua_fail(L, "sensor constant must be sdlinput.SENSOR_GYRO or sdlinput.SENSOR_ACCEL");
    }

    if (!SDL_SetGamepadSensorEnabled(gp, (SDL_SensorType)type, enabled)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int LuaGamepadIsSensorEnabled(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    int type = (int)luaL_checkinteger(L, 2);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushboolean(L, 0); return 1; }

    if (type != SDL_SENSOR_GYRO && type != SDL_SENSOR_ACCEL) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, SDL_GamepadSensorEnabled(gp, (SDL_SensorType)type));
    return 1;
}

static int LuaGamepadRumble(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    float left  = (float)luaL_checknumber(L, 2);
    float right = (float)luaL_checknumber(L, 3);
    Uint32 dur   = (Uint32)luaL_checkinteger(L, 4);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushboolean(L, 0); return 1; }
    if (!SDL_RumbleGamepad(gp, (Uint16)(left * 65535.0f), (Uint16)(right * 65535.0f), dur)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int LuaGamepadHasLed(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushboolean(L, 0); return 1; }
    SDL_PropertiesID props = SDL_GetGamepadProperties(gp);
    bool has = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
    lua_pushboolean(L, has);
    return 1;
}

static int LuaGamepadSetLed(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    Uint8 r = (Uint8)luaL_checkinteger(L, 2);
    Uint8 g = (Uint8)luaL_checkinteger(L, 3);
    Uint8 b = (Uint8)luaL_checkinteger(L, 4);
    if (!gp || !SDL_GamepadConnected(gp)) { lua_pushboolean(L, 0); return 1; }
    if (!SDL_SetGamepadLED(gp, r, g, b)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int LuaGamepadAddMapping(lua_State* L)
{
    const char* mapping = luaL_checkstring(L, 1);
    int result = SDL_AddGamepadMapping(mapping);
    if (result < 0) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------
// Module methods
// ---------------------------------------------------------------------------

static const luaL_reg Module_methods[] =
{
    {"init",                  LuaInit},
    {"quit",                  LuaQuit},
    {"pump_events",           LuaPumpEvents},
    {"num_gamepads",          LuaNumGamepads},
    {"gamepad_name",          LuaGamepadName},
    {"gamepad_open",          LuaGamepadOpen},
    {"gamepad_close",         LuaGamepadClose},
    {"gamepad_connected",     LuaGamepadConnected},
    {"gamepad_get_type",      LuaGamepadGetType},
    {"gamepad_get_vid",       LuaGamepadGetVid},
    {"gamepad_get_pid",       LuaGamepadGetPid},
    {"gamepad_get_buttons",   LuaGamepadGetButtons},
    {"gamepad_get_button",    LuaGamepadGetButton},
    {"gamepad_get_axis",      LuaGamepadGetAxis},
    {"gamepad_get_sensor",    LuaGamepadGetSensor},
    {"gamepad_set_sensor_enabled", LuaGamepadSetSensorEnabled},
    {"gamepad_is_sensor_enabled", LuaGamepadIsSensorEnabled},
    {"gamepad_rumble",        LuaGamepadRumble},
    {"gamepad_has_led",       LuaGamepadHasLed},
    {"gamepad_set_led",       LuaGamepadSetLed},
    {"gamepad_add_mapping",   LuaGamepadAddMapping},
    {0, 0}
};

static void push_constants(lua_State* L)
{
    static const Constant* all_arrays[] = {
        BUTTON_CONSTS, AXIS_CONSTS, SENSOR_CONSTS, TYPE_CONSTS, 0
    };

    for (int a = 0; all_arrays[a]; ++a) {
        for (int i = 0; all_arrays[a][i].name; ++i) {
            lua_pushinteger(L, all_arrays[a][i].value);
            lua_setfield(L, -2, all_arrays[a][i].name);
        }
    }
}

// ---------------------------------------------------------------------------
// Registration entry point
// ---------------------------------------------------------------------------

void sdlinput_register(lua_State* L)
{
    luaL_register(L, MODULE_NAME, Module_methods);
    push_constants(L);
    lua_pop(L, 1);
}
