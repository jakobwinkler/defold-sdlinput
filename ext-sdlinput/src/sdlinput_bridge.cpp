#include <dmsdk/sdk.h>
#include <string.h>
#include <stdlib.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hidapi.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_haptic.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_joystick.h>

#define MODULE_NAME "sdlinput"

// ---------------------------------------------------------------------------
// Handle tracking — simple flat arrays with linear scan
// ---------------------------------------------------------------------------

#define MAX_GAMEPADS 64
#define MAX_HID 64

static SDL_Gamepad*    g_gamepads[MAX_GAMEPADS];
static int             g_num_gamepads;
static SDL_hid_device* g_hid_devs[MAX_HID];
static int             g_num_hid_devs;

static void track_gamepad(SDL_Gamepad* gp)
{
    if (!gp) return;
    if (g_num_gamepads >= MAX_GAMEPADS) return;
    g_gamepads[g_num_gamepads++] = gp;
}

static void untrack_gamepad(SDL_Gamepad* gp)
{
    for (int i = 0; i < g_num_gamepads; ++i) {
        if (g_gamepads[i] == gp) {
            g_gamepads[i] = g_gamepads[--g_num_gamepads];
            return;
        }
    }
}

static void track_hid(SDL_hid_device* dev)
{
    if (!dev) return;
    if (g_num_hid_devs >= MAX_HID) return;
    g_hid_devs[g_num_hid_devs++] = dev;
}

static void untrack_hid(SDL_hid_device* dev)
{
    for (int i = 0; i < g_num_hid_devs; ++i) {
        if (g_hid_devs[i] == dev) {
            g_hid_devs[i] = g_hid_devs[--g_num_hid_devs];
            return;
        }
    }
}

void sdlinput_close_all_handles()
{
    for (int i = 0; i < g_num_gamepads; ++i) {
        SDL_CloseGamepad(g_gamepads[i]);
    }
    g_num_gamepads = 0;

    for (int i = 0; i < g_num_hid_devs; ++i) {
        SDL_hid_close(g_hid_devs[i]);
    }
    g_num_hid_devs = 0;
}

// ---------------------------------------------------------------------------
// Helper: push nil + error string onto Lua stack; return 1
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

// ---------------------------------------------------------------------------
// sdlinput.init()
// ---------------------------------------------------------------------------

static int LuaInit(lua_State* L)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.quit()
// ---------------------------------------------------------------------------

static int LuaQuit(lua_State* L)
{
    SDL_Quit();
    return 0;
}

// ---------------------------------------------------------------------------
// sdlinput.pump_events()
// ---------------------------------------------------------------------------

static int LuaPumpEvents(lua_State* L)
{
    SDL_PumpEvents();
    return 0;
}

// ---------------------------------------------------------------------------
// sdlinput.num_gamepads()
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// sdlinput.gamepad_name(index)
// ---------------------------------------------------------------------------

static int LuaGamepadName(lua_State* L)
{
    SDL_PumpEvents();
    int idx = (int)luaL_checkinteger(L, 1);  // 0-based
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return lua_fail_sdl(L);
    if (idx < 0 || idx >= count) {
        SDL_free(ids);
        lua_pushnil(L);
        lua_pushstring(L, "index out of range");
        return 2;
    }
    const char* name = SDL_GetGamepadNameForID(ids[idx]);
    SDL_free(ids);
    if (!name) return lua_fail_sdl(L);
    lua_pushstring(L, name);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_open(index)  → handle (lightuserdata)
// ---------------------------------------------------------------------------

static int LuaGamepadOpen(lua_State* L)
{
    SDL_PumpEvents();
    int idx = (int)luaL_checkinteger(L, 1);  // 0-based
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return lua_fail_sdl(L);
    if (idx < 0 || idx >= count) {
        SDL_free(ids);
        lua_pushnil(L);
        lua_pushstring(L, "index out of range");
        return 2;
    }
    SDL_Gamepad* gp = SDL_OpenGamepad(ids[idx]);
    SDL_free(ids);
    if (!gp) return lua_fail_sdl(L);
    track_gamepad(gp);
    lua_pushlightuserdata(L, gp);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_close(handle)
// ---------------------------------------------------------------------------

static int LuaGamepadClose(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (gp) {
        SDL_CloseGamepad(gp);
        untrack_gamepad(gp);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_connected(handle) → bool
// ---------------------------------------------------------------------------

static int LuaGamepadConnected(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    lua_pushboolean(L, gp ? SDL_GamepadConnected(gp) : 0);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_get_type(handle) → string
// ---------------------------------------------------------------------------

static int LuaGamepadGetType(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp) {
        lua_pushstring(L, "unknown");
        return 1;
    }
    SDL_GamepadType t = SDL_GetGamepadType(gp);
    const char* s = SDL_GetGamepadStringForType(t);
    lua_pushstring(L, s ? s : "unknown");
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_get_vid(handle) → number
// ---------------------------------------------------------------------------

static int LuaGamepadGetVid(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, SDL_GetGamepadVendor(gp));
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_get_pid(handle) → number
// ---------------------------------------------------------------------------

static int LuaGamepadGetPid(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, SDL_GetGamepadProduct(gp));
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_get_buttons(handle) → table
// ---------------------------------------------------------------------------

struct ButtonMap {
    const char* name;
    SDL_GamepadButton btn;
};

static const ButtonMap BUTTON_MAP[] = {
    {"a",              SDL_GAMEPAD_BUTTON_SOUTH},
    {"b",              SDL_GAMEPAD_BUTTON_EAST},
    {"x",              SDL_GAMEPAD_BUTTON_WEST},
    {"y",              SDL_GAMEPAD_BUTTON_NORTH},
    {"dpad_up",        SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"dpad_down",      SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"dpad_left",      SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"dpad_right",     SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"left_shoulder",  SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"right_shoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"left_stick",     SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"right_stick",    SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"start",          SDL_GAMEPAD_BUTTON_START},
    {"back",           SDL_GAMEPAD_BUTTON_BACK},
    {"guide",          SDL_GAMEPAD_BUTTON_GUIDE},
    {"misc1",          SDL_GAMEPAD_BUTTON_MISC1},
    {"paddle1",        SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1},
    {"paddle2",        SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2},
    {"paddle3",        SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
    {"paddle4",        SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
    {"touchpad",       SDL_GAMEPAD_BUTTON_TOUCHPAD},
    {NULL, SDL_GAMEPAD_BUTTON_INVALID}
};

static int LuaGamepadGetButtons(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp) {
        lua_pushnil(L);
        return 1;
    }

    SDL_PumpEvents();
    lua_createtable(L, 0, 21);
    for (int i = 0; BUTTON_MAP[i].name; ++i) {
        bool pressed = SDL_GetGamepadButton(gp, BUTTON_MAP[i].btn);
        lua_pushboolean(L, pressed);
        lua_setfield(L, -2, BUTTON_MAP[i].name);
    }
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_get_axis(handle, axis_string) → float
// ---------------------------------------------------------------------------

struct AxisMap {
    const char* name;
    SDL_GamepadAxis axis;
};

static const AxisMap AXIS_MAP[] = {
    {"leftx",         SDL_GAMEPAD_AXIS_LEFTX},
    {"lefty",         SDL_GAMEPAD_AXIS_LEFTY},
    {"rightx",        SDL_GAMEPAD_AXIS_RIGHTX},
    {"righty",        SDL_GAMEPAD_AXIS_RIGHTY},
    {"left_trigger",  SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
    {"right_trigger", SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
    {NULL, SDL_GAMEPAD_AXIS_INVALID}
};

static int LuaGamepadGetAxis(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    const char* axis_name = luaL_checkstring(L, 2);
    if (!gp) { lua_pushnumber(L, 0); return 1; }

    SDL_PumpEvents();

    SDL_GamepadAxis axis = SDL_GetGamepadAxisFromString(axis_name);
    if (axis == SDL_GAMEPAD_AXIS_INVALID) {
        // fallback: manual lookup
        for (int i = 0; AXIS_MAP[i].name; ++i) {
            if (strcmp(AXIS_MAP[i].name, axis_name) == 0) {
                axis = AXIS_MAP[i].axis;
                break;
            }
        }
        if (axis == SDL_GAMEPAD_AXIS_INVALID) {
            lua_pushnumber(L, 0);
            return 1;
        }
    }

    Sint16 val = SDL_GetGamepadAxis(gp, axis);
    float normalized;
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        // Triggers: 0..32767 → 0..1
        normalized = val / 32767.0f;
        if (normalized < 0) normalized = 0;
    } else {
        // Sticks: -32768..32767 → -1..1
        normalized = val / 32767.0f;
        if (normalized < -1.0f) normalized = -1.0f;
    }
    lua_pushnumber(L, normalized);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_get_sensor(handle, type_string) → {x,y,z}
// ---------------------------------------------------------------------------

static int LuaGamepadGetSensor(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    const char* type_name = luaL_checkstring(L, 2);
    if (!gp) { lua_pushnil(L); return 1; }

    SDL_SensorType st = SDL_SENSOR_INVALID;
    if (strcmp(type_name, "gyro") == 0)       st = SDL_SENSOR_GYRO;
    else if (strcmp(type_name, "accel") == 0) st = SDL_SENSOR_ACCEL;

    if (st == SDL_SENSOR_INVALID) {
        return lua_fail(L, "unknown sensor type, use 'gyro' or 'accel'");
    }

    SDL_PumpEvents();
    float data[3] = {0};
    if (!SDL_GetGamepadSensorData(gp, st, data, 3)) {
        return lua_fail_sdl(L);
    }
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, data[0]); lua_setfield(L, -2, "x");
    lua_pushnumber(L, data[1]); lua_setfield(L, -2, "y");
    lua_pushnumber(L, data[2]); lua_setfield(L, -2, "z");
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_set_sensor_enabled(handle, type_string, enabled) → bool
// ---------------------------------------------------------------------------

static int LuaGamepadSetSensorEnabled(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    const char* type_name = luaL_checkstring(L, 2);
    bool enabled = lua_toboolean(L, 3) != 0;
    if (!gp) { lua_pushboolean(L, 0); return 1; }

    SDL_SensorType st = SDL_SENSOR_INVALID;
    if (strcmp(type_name, "gyro") == 0)       st = SDL_SENSOR_GYRO;
    else if (strcmp(type_name, "accel") == 0) st = SDL_SENSOR_ACCEL;

    if (st == SDL_SENSOR_INVALID) {
        return lua_fail(L, "unknown sensor type, use 'gyro' or 'accel'");
    }

    if (!SDL_SetGamepadSensorEnabled(gp, st, enabled)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_is_sensor_enabled(handle, type_string) → bool
// ---------------------------------------------------------------------------

static int LuaGamepadIsSensorEnabled(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    const char* type_name = luaL_checkstring(L, 2);
    if (!gp) { lua_pushboolean(L, 0); return 1; }

    SDL_SensorType st = SDL_SENSOR_INVALID;
    if (strcmp(type_name, "gyro") == 0)       st = SDL_SENSOR_GYRO;
    else if (strcmp(type_name, "accel") == 0) st = SDL_SENSOR_ACCEL;

    if (st == SDL_SENSOR_INVALID) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, SDL_GamepadSensorEnabled(gp, st));
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_rumble(handle, left_motor, right_motor, duration_ms)
// ---------------------------------------------------------------------------

static int LuaGamepadRumble(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    float left  = (float)luaL_checknumber(L, 2);
    float right = (float)luaL_checknumber(L, 3);
    Uint32 dur   = (Uint32)luaL_checkinteger(L, 4);
    if (!gp) { lua_pushboolean(L, 0); return 1; }
    if (!SDL_RumbleGamepad(gp, (Uint16)(left * 65535.0f), (Uint16)(right * 65535.0f), dur)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_has_led(handle) → bool
// ---------------------------------------------------------------------------

static int LuaGamepadHasLed(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    if (!gp) { lua_pushboolean(L, 0); return 1; }
    SDL_PropertiesID props = SDL_GetGamepadProperties(gp);
    bool has = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
    lua_pushboolean(L, has);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_set_led(handle, r, g, b)
// ---------------------------------------------------------------------------

static int LuaGamepadSetLed(lua_State* L)
{
    SDL_Gamepad* gp = (SDL_Gamepad*)lua_touserdata(L, 1);
    Uint8 r = (Uint8)luaL_checkinteger(L, 2);
    Uint8 g = (Uint8)luaL_checkinteger(L, 3);
    Uint8 b = (Uint8)luaL_checkinteger(L, 4);
    if (!gp) { lua_pushboolean(L, 0); return 1; }
    if (!SDL_SetGamepadLED(gp, r, g, b)) {
        return lua_fail_sdl(L);
    }
    lua_pushboolean(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------
// sdlinput.gamepad_add_mapping(mapping_string)
// ---------------------------------------------------------------------------

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
// HID API
// ---------------------------------------------------------------------------

// sdlinput.hid_enumerate(vid, pid) → table|nil, err
static int LuaHidEnumerate(lua_State* L)
{
    unsigned short vid = (unsigned short)luaL_checkinteger(L, 1);
    unsigned short pid = (unsigned short)luaL_checkinteger(L, 2);

    SDL_hid_device_info* devs = SDL_hid_enumerate(vid, pid);
    if (!devs) {
        // not an error — just no devices
        lua_createtable(L, 0, 0);
        return 1;
    }

    lua_createtable(L, 0, 0); // array
    int idx = 1;
    for (SDL_hid_device_info* cur = devs; cur; cur = cur->next) {
        lua_createtable(L, 0, 10);
        lua_pushstring(L, cur->path ? cur->path : "");
        lua_setfield(L, -2, "path");
        lua_pushinteger(L, cur->vendor_id);
        lua_setfield(L, -2, "vendor_id");
        lua_pushinteger(L, cur->product_id);
        lua_setfield(L, -2, "product_id");
        lua_pushinteger(L, cur->usage_page);
        lua_setfield(L, -2, "usage_page");
        lua_pushinteger(L, cur->usage);
        lua_setfield(L, -2, "usage");
        // short string from wchar_t serial_number (just first 64 chars)
        char serial_buf[256] = {0};
        if (cur->serial_number) {
            wcstombs(serial_buf, cur->serial_number, sizeof(serial_buf) - 1);
        }
        lua_pushstring(L, serial_buf);
        lua_setfield(L, -2, "serial_number");
        // manufacturer_string
        {
            char mbuf[256] = {0};
            if (cur->manufacturer_string)
                wcstombs(mbuf, cur->manufacturer_string, sizeof(mbuf)-1);
            lua_pushstring(L, mbuf);
            lua_setfield(L, -2, "manufacturer_string");
        }
        // product_string
        {
            char mbuf[256] = {0};
            if (cur->product_string)
                wcstombs(mbuf, cur->product_string, sizeof(mbuf)-1);
            lua_pushstring(L, mbuf);
            lua_setfield(L, -2, "product_string");
        }
        // interface_number
        lua_pushinteger(L, cur->interface_number);
        lua_setfield(L, -2, "interface_number");
        // release_number
        lua_pushinteger(L, cur->release_number);
        lua_setfield(L, -2, "release_number");
        lua_rawseti(L, -2, idx++);
    }
    SDL_hid_free_enumeration(devs);
    return 1;
}

// sdlinput.hid_open(vid, pid) → handle|nil, err
static int LuaHidOpen(lua_State* L)
{
    unsigned short vid = (unsigned short)luaL_checkinteger(L, 1);
    unsigned short pid = (unsigned short)luaL_checkinteger(L, 2);
    SDL_hid_device* dev = SDL_hid_open(vid, pid, NULL);
    if (!dev) return lua_fail_sdl(L);
    track_hid(dev);
    lua_pushlightuserdata(L, dev);
    return 1;
}

// sdlinput.hid_open_path(path) → handle|nil, err
static int LuaHidOpenPath(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    SDL_hid_device* dev = SDL_hid_open_path(path);
    if (!dev) return lua_fail_sdl(L);
    track_hid(dev);
    lua_pushlightuserdata(L, dev);
    return 1;
}

// sdlinput.hid_close(handle)
static int LuaHidClose(lua_State* L)
{
    SDL_hid_device* dev = (SDL_hid_device*)lua_touserdata(L, 1);
    if (dev) {
        SDL_hid_close(dev);
        untrack_hid(dev);
    }
    return 0;
}

// sdlinput.hid_read(handle, len, timeout_ms) → string|nil, err
static int LuaHidRead(lua_State* L)
{
    SDL_hid_device* dev = (SDL_hid_device*)lua_touserdata(L, 1);
    size_t len = (size_t)luaL_checkinteger(L, 2);
    int timeout = (int)luaL_checkinteger(L, 3);
    if (!dev) return lua_fail(L, "invalid HID handle");

    // Allocate buffer on heap to avoid stack overflow
    unsigned char* buf = (unsigned char*)malloc(len);
    if (!buf) return lua_fail(L, "out of memory");

    int n;
    if (timeout >= 0) {
        n = SDL_hid_read_timeout(dev, buf, len, timeout);
    } else {
        n = SDL_hid_read(dev, buf, len);
    }
    if (n < 0) {
        free(buf);
        return lua_fail_sdl(L);
    }
    lua_pushlstring(L, (const char*)buf, (size_t)n);
    free(buf);
    return 1;
}

// sdlinput.hid_write(handle, data_string) → count|nil, err
static int LuaHidWrite(lua_State* L)
{
    SDL_hid_device* dev = (SDL_hid_device*)lua_touserdata(L, 1);
    size_t dlen = 0;
    const unsigned char* data = (const unsigned char*)luaL_checklstring(L, 2, &dlen);
    if (!dev) return lua_fail(L, "invalid HID handle");

    int n = SDL_hid_write(dev, data, dlen);
    if (n < 0) return lua_fail_sdl(L);
    lua_pushinteger(L, n);
    return 1;
}

// sdlinput.hid_get_feature_report(handle, report_id, len) → string|nil, err
static int LuaHidGetFeatureReport(lua_State* L)
{
    SDL_hid_device* dev = (SDL_hid_device*)lua_touserdata(L, 1);
    int report_id = (int)luaL_checkinteger(L, 2);
    size_t len = (size_t)luaL_checkinteger(L, 3);
    if (!dev) return lua_fail(L, "invalid HID handle");

    unsigned char* buf = (unsigned char*)malloc(len + 1);
    if (!buf) return lua_fail(L, "out of memory");
    buf[0] = (unsigned char)report_id;

    int n = SDL_hid_get_feature_report(dev, buf, len + 1);
    if (n < 0) {
        free(buf);
        return lua_fail_sdl(L);
    }
    if (n <= 1) {
        free(buf);
        lua_pushliteral(L, "");
        return 1;
    }
    // Return data starting at byte 1 (skip report id byte which is unchanged)
    lua_pushlstring(L, (const char*)buf + 1, (size_t)(n - 1));
    free(buf);
    return 1;
}

// sdlinput.hid_send_feature_report(handle, data_string) → count|nil, err
static int LuaHidSendFeatureReport(lua_State* L)
{
    SDL_hid_device* dev = (SDL_hid_device*)lua_touserdata(L, 1);
    size_t dlen = 0;
    const unsigned char* data = (const unsigned char*)luaL_checklstring(L, 2, &dlen);
    if (!dev) return lua_fail(L, "invalid HID handle");

    int n = SDL_hid_send_feature_report(dev, data, dlen);
    if (n < 0) return lua_fail_sdl(L);
    lua_pushinteger(L, n);
    return 1;
}

// sdlinput.hid_error(handle) → string
static int LuaHidError(lua_State* L)
{
    SDL_hid_device* dev = (SDL_hid_device*)lua_touserdata(L, 1);
    if (!dev) { lua_pushstring(L, ""); return 1; }
    lua_pushstring(L, SDL_GetError());
    return 1;
}

// ---------------------------------------------------------------------------
// Module methods array
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
    {"gamepad_get_axis",      LuaGamepadGetAxis},
    {"gamepad_get_sensor",    LuaGamepadGetSensor},
    {"gamepad_set_sensor_enabled", LuaGamepadSetSensorEnabled},
    {"gamepad_is_sensor_enabled", LuaGamepadIsSensorEnabled},
    {"gamepad_rumble",        LuaGamepadRumble},
    {"gamepad_has_led",       LuaGamepadHasLed},
    {"gamepad_set_led",       LuaGamepadSetLed},
    {"gamepad_add_mapping",   LuaGamepadAddMapping},
    {"hid_enumerate",         LuaHidEnumerate},
    {"hid_open",              LuaHidOpen},
    {"hid_open_path",         LuaHidOpenPath},
    {"hid_close",             LuaHidClose},
    {"hid_read",              LuaHidRead},
    {"hid_write",             LuaHidWrite},
    {"hid_get_feature_report", LuaHidGetFeatureReport},
    {"hid_send_feature_report", LuaHidSendFeatureReport},
    {"hid_error",             LuaHidError},
    {0, 0}
};

// ---------------------------------------------------------------------------
// Registration entry point (called from extension.cpp)
// ---------------------------------------------------------------------------

void sdlinput_register(lua_State* L)
{
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
}
