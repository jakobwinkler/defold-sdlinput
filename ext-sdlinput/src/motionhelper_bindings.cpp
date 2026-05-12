#include <dmsdk/sdk.h>
#include <string.h>

#include "GamepadMotion.hpp"

#define MODULE_NAME "motionhelper"
#define METATABLE_NAME "motionhelper"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static int lua_fail(lua_State* L, const char* err)
{
    lua_pushnil(L);
    lua_pushstring(L, err ? err : "unknown error");
    return 2;
}

// Get GamepadMotion* from full userdata at stack index idx
// If invalid (type mismatch or null), returns nullptr and pushes error
static GamepadMotion* check_motion(lua_State* L, int idx)
{
    void* ud = luaL_checkudata(L, idx, METATABLE_NAME);
    if (!ud) return nullptr;
    GamepadMotion* m = *(GamepadMotion**)ud;
    if (!m) {
        lua_pushnil(L);
        lua_pushstring(L, "motionhelper handle has been freed");
    }
    return m;
}

// ---------------------------------------------------------------------------
// CalibrationMode string <-> enum helpers
// ---------------------------------------------------------------------------

static const char* mode_to_string(GamepadMotionHelpers::CalibrationMode mode)
{
    switch (mode) {
        case GamepadMotionHelpers::CalibrationMode::Manual:       return "manual";
        case GamepadMotionHelpers::CalibrationMode::Stillness:    return "stillness";
        case GamepadMotionHelpers::CalibrationMode::SensorFusion: return "sensor_fusion";
        default: return "manual";
    }
}

static GamepadMotionHelpers::CalibrationMode string_to_mode(const char* s)
{
    if (strcmp(s, "manual") == 0)        return GamepadMotionHelpers::CalibrationMode::Manual;
    if (strcmp(s, "stillness") == 0)     return GamepadMotionHelpers::CalibrationMode::Stillness;
    if (strcmp(s, "sensor_fusion") == 0) return GamepadMotionHelpers::CalibrationMode::SensorFusion;
    if (strcmp(s, "stillness|sensor_fusion") == 0)
        return GamepadMotionHelpers::CalibrationMode::Stillness | GamepadMotionHelpers::CalibrationMode::SensorFusion;
    return GamepadMotionHelpers::CalibrationMode::Manual;
}

static int LuaMotionNew(lua_State* L)
{
    GamepadMotion* m = new GamepadMotion();
    if (!m) return lua_fail(L, "out of memory");

    // Allocate full userdata to hold the pointer, giving each handle its own metatable
    GamepadMotion** ud = (GamepadMotion**)lua_newuserdata(L, sizeof(GamepadMotion*));
    *ud = m;

    luaL_getmetatable(L, METATABLE_NAME);
    lua_setmetatable(L, -2);
    return 1;
}

static int LuaMotionGc(lua_State* L)
{
    GamepadMotion** ud = (GamepadMotion**)luaL_checkudata(L, 1, METATABLE_NAME);
    if (ud && *ud) {
        delete *ud;
        *ud = nullptr;
    }
    return 0;
}

static int LuaMotionFree(lua_State* L)
{
    GamepadMotion** ud = (GamepadMotion**)luaL_checkudata(L, 1, METATABLE_NAME);
    if (ud && *ud) {
        delete *ud;
        *ud = nullptr;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// motionhelper:process(gyro, accel, dt)
// gyro and accel are tables with x, y, z fields
// ---------------------------------------------------------------------------

static int LuaMotionProcess(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    if (!lua_istable(L, 2)) return lua_fail(L, "arg 1: expected gyro table {x,y,z}");
    lua_getfield(L, 2, "x"); float gx = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 2, "y"); float gy = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 2, "z"); float gz = (float)lua_tonumber(L, -1); lua_pop(L, 1);

    if (!lua_istable(L, 3)) return lua_fail(L, "arg 2: expected accel table {x,y,z}");
    lua_getfield(L, 3, "x"); float ax = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 3, "y"); float ay = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 3, "z"); float az = (float)lua_tonumber(L, -1); lua_pop(L, 1);

    float dt = (float)luaL_checknumber(L, 4);

    m->ProcessMotion(gx, gy, gz, ax, ay, az, dt);
    return 0;
}

// ---------------------------------------------------------------------------
// motionhelper:get_state() → table
// Returns one table with ALL commonly-needed values.
// Fields: ow,ox,oy,oz (orientation quat), gx,gy,gz (gravity),
// ax,ay,az (processed accel), cgx,cgy,cgz (calibrated gyro),
// psx,psy (player-space gyro), wsx,wsy (world-space gyro),
// confidence (float), mode (string)
// ---------------------------------------------------------------------------

static int LuaMotionGetState(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float ow, ox, oy, oz;
    m->GetOrientation(ow, ox, oy, oz);

    float gx, gy, gz;
    m->GetGravity(gx, gy, gz);

    float ax, ay, az;
    m->GetProcessedAcceleration(ax, ay, az);

    float cgx, cgy, cgz;
    m->GetCalibratedGyro(cgx, cgy, cgz);

    float psx, psy;
    m->GetPlayerSpaceGyro(psx, psy);

    float wsx, wsy;
    m->GetWorldSpaceGyro(wsx, wsy);

    float confidence = m->GetAutoCalibrationConfidence();
    const char* mode = mode_to_string(m->GetCalibrationMode());

    lua_createtable(L, 0, 23);

    lua_pushnumber(L, ow); lua_setfield(L, -2, "ow");
    lua_pushnumber(L, ox); lua_setfield(L, -2, "ox");
    lua_pushnumber(L, oy); lua_setfield(L, -2, "oy");
    lua_pushnumber(L, oz); lua_setfield(L, -2, "oz");

    lua_pushnumber(L, gx); lua_setfield(L, -2, "gx");
    lua_pushnumber(L, gy); lua_setfield(L, -2, "gy");
    lua_pushnumber(L, gz); lua_setfield(L, -2, "gz");

    lua_pushnumber(L, ax); lua_setfield(L, -2, "ax");
    lua_pushnumber(L, ay); lua_setfield(L, -2, "ay");
    lua_pushnumber(L, az); lua_setfield(L, -2, "az");

    lua_pushnumber(L, cgx); lua_setfield(L, -2, "cgx");
    lua_pushnumber(L, cgy); lua_setfield(L, -2, "cgy");
    lua_pushnumber(L, cgz); lua_setfield(L, -2, "cgz");

    lua_pushnumber(L, psx); lua_setfield(L, -2, "psx");
    lua_pushnumber(L, psy); lua_setfield(L, -2, "psy");

    lua_pushnumber(L, wsx); lua_setfield(L, -2, "wsx");
    lua_pushnumber(L, wsy); lua_setfield(L, -2, "wsy");

    lua_pushnumber(L, confidence); lua_setfield(L, -2, "confidence");
    lua_pushstring(L, mode); lua_setfield(L, -2, "mode");

    return 1;
}

// ---------------------------------------------------------------------------
// Individual getters (still available for when you only need one value)
// ---------------------------------------------------------------------------

// motionhelper:get_orientation() → w, x, y, z
static int LuaMotionGetOrientation(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float w, x, y, z;
    m->GetOrientation(w, x, y, z);
    lua_pushnumber(L, w);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 4;
}

// motionhelper:get_gravity() → x, y, z
static int LuaMotionGetGravity(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float x, y, z;
    m->GetGravity(x, y, z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

// motionhelper:get_processed_acceleration() → x, y, z
static int LuaMotionGetProcessedAcceleration(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float x, y, z;
    m->GetProcessedAcceleration(x, y, z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

// motionhelper:get_calibrated_gyro() → x, y, z
static int LuaMotionGetCalibratedGyro(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float x, y, z;
    m->GetCalibratedGyro(x, y, z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

// motionhelper:get_player_space_gyro([yawRelaxFactor]) → x, y
static int LuaMotionGetPlayerSpaceGyro(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float yawRelaxFactor = (float)luaL_optnumber(L, 2, 1.41f);
    float x, y;
    m->GetPlayerSpaceGyro(x, y, yawRelaxFactor);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// motionhelper:get_world_space_gyro([sideReductionThreshold]) → x, y
static int LuaMotionGetWorldSpaceGyro(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float sideReductionThreshold = (float)luaL_optnumber(L, 2, 0.125f);
    float x, y;
    m->GetWorldSpaceGyro(x, y, sideReductionThreshold);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// ---------------------------------------------------------------------------
// Calibration controls
// ---------------------------------------------------------------------------

// motionhelper:start_calibration()
static int LuaMotionStartCalibration(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;
    m->StartContinuousCalibration();
    return 0;
}

// motionhelper:pause_calibration()
static int LuaMotionPauseCalibration(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;
    m->PauseContinuousCalibration();
    return 0;
}

// motionhelper:reset_calibration()
static int LuaMotionResetCalibration(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;
    m->ResetContinuousCalibration();
    return 0;
}

// motionhelper:get_calibration_offset() → x, y, z
static int LuaMotionGetCalibrationOffset(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float x, y, z;
    m->GetCalibrationOffset(x, y, z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

// motionhelper:set_calibration_offset(x, y, z, weight)
static int LuaMotionSetCalibrationOffset(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);
    int weight = (int)luaL_checkinteger(L, 5);

    m->SetCalibrationOffset(x, y, z, weight);
    return 0;
}

// motionhelper:get_auto_calibration_confidence() → float
static int LuaMotionGetAutoCalibrationConfidence(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    lua_pushnumber(L, m->GetAutoCalibrationConfidence());
    return 1;
}

// motionhelper:set_auto_calibration_confidence(conf)
static int LuaMotionSetAutoCalibrationConfidence(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    float conf = (float)luaL_checknumber(L, 2);
    m->SetAutoCalibrationConfidence(conf);
    return 0;
}

// motionhelper:get_auto_calibration_is_steady() → bool
static int LuaMotionGetAutoCalibrationIsSteady(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    lua_pushboolean(L, m->GetAutoCalibrationIsSteady());
    return 1;
}

// motionhelper:set_calibration_mode(mode_string)
static int LuaMotionSetCalibrationMode(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    const char* s = luaL_checkstring(L, 2);
    m->SetCalibrationMode(string_to_mode(s));
    return 0;
}

// motionhelper:get_calibration_mode() → string
static int LuaMotionGetCalibrationMode(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    lua_pushstring(L, mode_to_string(m->GetCalibrationMode()));
    return 1;
}

static int LuaMotionResetMotion(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    m->ResetMotion();
    return 0;
}

static int LuaMotionReset(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    m->Reset();
    return 0;
}

// ---------------------------------------------------------------------------
// Settings access
// ---------------------------------------------------------------------------

// motionhelper:set_setting(key, value)
static int LuaMotionSetSetting(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    const char* key = luaL_checkstring(L, 2);
    float val = (float)luaL_checknumber(L, 3);

#define SET_FIELD(field) do { \
    if (strcmp(key, #field) == 0) { m->Settings.field = val; return 0; } \
} while(0)

    SET_FIELD(MinStillnessSamples);
    SET_FIELD(MinStillnessCollectionTime);
    SET_FIELD(MinStillnessCorrectionTime);
    SET_FIELD(MaxStillnessError);
    SET_FIELD(StillnessSampleDeteriorationRate);
    SET_FIELD(StillnessErrorClimbRate);
    SET_FIELD(StillnessErrorDropOnRecalibrate);
    SET_FIELD(StillnessCalibrationEaseInTime);
    SET_FIELD(StillnessCalibrationHalfTime);
    SET_FIELD(StillnessConfidenceRate);
    SET_FIELD(StillnessGyroDelta);
    SET_FIELD(StillnessAccelDelta);
    SET_FIELD(SensorFusionCalibrationSmoothingStrength);
    SET_FIELD(SensorFusionAngularAccelerationThreshold);
    SET_FIELD(SensorFusionCalibrationEaseInTime);
    SET_FIELD(SensorFusionCalibrationHalfTime);
    SET_FIELD(SensorFusionConfidenceRate);
    SET_FIELD(GravityCorrectionShakinessMaxThreshold);
    SET_FIELD(GravityCorrectionShakinessMinThreshold);
    SET_FIELD(GravityCorrectionStillSpeed);
    SET_FIELD(GravityCorrectionShakySpeed);
    SET_FIELD(GravityCorrectionGyroFactor);
    SET_FIELD(GravityCorrectionGyroMinThreshold);
    SET_FIELD(GravityCorrectionGyroMaxThreshold);
    SET_FIELD(GravityCorrectionMinimumSpeed);

#undef SET_FIELD

    return lua_fail(L, "unknown setting key");
}

// motionhelper:get_setting(key) → number
static int LuaMotionGetSetting(lua_State* L)
{
    GamepadMotion* m = check_motion(L, 1);
    if (!m) return 2;

    const char* key = luaL_checkstring(L, 2);

#define GET_FIELD(field) do { \
    if (strcmp(key, #field) == 0) { lua_pushnumber(L, (float)m->Settings.field); return 1; } \
} while(0)

    GET_FIELD(MinStillnessSamples);
    GET_FIELD(MinStillnessCollectionTime);
    GET_FIELD(MinStillnessCorrectionTime);
    GET_FIELD(MaxStillnessError);
    GET_FIELD(StillnessSampleDeteriorationRate);
    GET_FIELD(StillnessErrorClimbRate);
    GET_FIELD(StillnessErrorDropOnRecalibrate);
    GET_FIELD(StillnessCalibrationEaseInTime);
    GET_FIELD(StillnessCalibrationHalfTime);
    GET_FIELD(StillnessConfidenceRate);
    GET_FIELD(StillnessGyroDelta);
    GET_FIELD(StillnessAccelDelta);
    GET_FIELD(SensorFusionCalibrationSmoothingStrength);
    GET_FIELD(SensorFusionAngularAccelerationThreshold);
    GET_FIELD(SensorFusionCalibrationEaseInTime);
    GET_FIELD(SensorFusionCalibrationHalfTime);
    GET_FIELD(SensorFusionConfidenceRate);
    GET_FIELD(GravityCorrectionShakinessMaxThreshold);
    GET_FIELD(GravityCorrectionShakinessMinThreshold);
    GET_FIELD(GravityCorrectionStillSpeed);
    GET_FIELD(GravityCorrectionShakySpeed);
    GET_FIELD(GravityCorrectionGyroFactor);
    GET_FIELD(GravityCorrectionGyroMinThreshold);
    GET_FIELD(GravityCorrectionGyroMaxThreshold);
    GET_FIELD(GravityCorrectionMinimumSpeed);

#undef GET_FIELD

    return lua_fail(L, "unknown setting key");
}

// ---------------------------------------------------------------------------
// Static helpers (no handle needed)
// ---------------------------------------------------------------------------

// motionhelper.calculate_player_space_gyro(gx, gy, gz, gravX, gravY, gravZ, [yawRelaxFactor]) → x, y
static int LuaMotionStaticPlayerSpaceGyro(lua_State* L)
{
    float gx  = (float)luaL_checknumber(L, 1);
    float gy  = (float)luaL_checknumber(L, 2);
    float gz  = (float)luaL_checknumber(L, 3);
    float grx = (float)luaL_checknumber(L, 4);
    float gry = (float)luaL_checknumber(L, 5);
    float grz = (float)luaL_checknumber(L, 6);
    float yawRelaxFactor = (float)luaL_optnumber(L, 7, 1.41f);

    float x, y;
    GamepadMotion::CalculatePlayerSpaceGyro(x, y, gx, gy, gz, grx, gry, grz, yawRelaxFactor);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// motionhelper.calculate_world_space_gyro(gx, gy, gz, gravX, gravY, gravZ, [sideReductionThreshold]) → x, y
static int LuaMotionStaticWorldSpaceGyro(lua_State* L)
{
    float gx  = (float)luaL_checknumber(L, 1);
    float gy  = (float)luaL_checknumber(L, 2);
    float gz  = (float)luaL_checknumber(L, 3);
    float grx = (float)luaL_checknumber(L, 4);
    float gry = (float)luaL_checknumber(L, 5);
    float grz = (float)luaL_checknumber(L, 6);
    float sideReductionThreshold = (float)luaL_optnumber(L, 7, 0.125f);

    float x, y;
    GamepadMotion::CalculateWorldSpaceGyro(x, y, gx, gy, gz, grx, gry, grz, sideReductionThreshold);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// ---------------------------------------------------------------------------
// Methods to go into the metatable (these handle : syntax)
// ---------------------------------------------------------------------------

static const luaL_reg Meta_methods[] =
{
    {"process",                       LuaMotionProcess},
    {"get_state",                     LuaMotionGetState},
    {"get_orientation",               LuaMotionGetOrientation},
    {"get_gravity",                   LuaMotionGetGravity},
    {"get_processed_acceleration",    LuaMotionGetProcessedAcceleration},
    {"get_calibrated_gyro",           LuaMotionGetCalibratedGyro},
    {"get_player_space_gyro",         LuaMotionGetPlayerSpaceGyro},
    {"get_world_space_gyro",          LuaMotionGetWorldSpaceGyro},
    {"start_calibration",             LuaMotionStartCalibration},
    {"pause_calibration",             LuaMotionPauseCalibration},
    {"reset_calibration",             LuaMotionResetCalibration},
    {"get_calibration_offset",        LuaMotionGetCalibrationOffset},
    {"set_calibration_offset",        LuaMotionSetCalibrationOffset},
    {"get_auto_calibration_confidence", LuaMotionGetAutoCalibrationConfidence},
    {"set_auto_calibration_confidence", LuaMotionSetAutoCalibrationConfidence},
    {"get_auto_calibration_is_steady", LuaMotionGetAutoCalibrationIsSteady},
    {"set_calibration_mode",          LuaMotionSetCalibrationMode},
    {"get_calibration_mode",          LuaMotionGetCalibrationMode},
    {"reset_motion",                  LuaMotionResetMotion},
    {"reset",                         LuaMotionReset},
    {"set_setting",                   LuaMotionSetSetting},
    {"get_setting",                   LuaMotionGetSetting},
    {"free",                          LuaMotionFree},
    {0, 0}
};

// ---------------------------------------------------------------------------
// Module-level functions (motionhelper.method(...) style)
// ---------------------------------------------------------------------------

static const luaL_reg Module_methods[] =
{
    {"new",                           LuaMotionNew},
    {"free",                          LuaMotionFree},
    {"process",                       LuaMotionProcess},
    {"get_state",                     LuaMotionGetState},
    {"get_orientation",               LuaMotionGetOrientation},
    {"get_gravity",                   LuaMotionGetGravity},
    {"get_processed_acceleration",    LuaMotionGetProcessedAcceleration},
    {"get_calibrated_gyro",           LuaMotionGetCalibratedGyro},
    {"get_player_space_gyro",         LuaMotionGetPlayerSpaceGyro},
    {"get_world_space_gyro",          LuaMotionGetWorldSpaceGyro},
    {"start_calibration",             LuaMotionStartCalibration},
    {"pause_calibration",             LuaMotionPauseCalibration},
    {"reset_calibration",             LuaMotionResetCalibration},
    {"get_calibration_offset",        LuaMotionGetCalibrationOffset},
    {"set_calibration_offset",        LuaMotionSetCalibrationOffset},
    {"get_auto_calibration_confidence", LuaMotionGetAutoCalibrationConfidence},
    {"set_auto_calibration_confidence", LuaMotionSetAutoCalibrationConfidence},
    {"get_auto_calibration_is_steady", LuaMotionGetAutoCalibrationIsSteady},
    {"set_calibration_mode",          LuaMotionSetCalibrationMode},
    {"get_calibration_mode",          LuaMotionGetCalibrationMode},
    {"reset_motion",                  LuaMotionResetMotion},
    {"reset",                         LuaMotionReset},
    {"set_setting",                   LuaMotionSetSetting},
    {"get_setting",                   LuaMotionGetSetting},
    {"calculate_player_space_gyro",   LuaMotionStaticPlayerSpaceGyro},
    {"calculate_world_space_gyro",    LuaMotionStaticWorldSpaceGyro},
    {0, 0}
};

// ---------------------------------------------------------------------------
// Registration entry point (called from extension.cpp)
// ---------------------------------------------------------------------------

void motionhelper_register(lua_State* L)
{
    // Create metatable for handles (individual per-object metatable)
    luaL_newmetatable(L, METATABLE_NAME);

    // metatable.__index = metatable (so :method lookup works)
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // __gc for auto-free
    lua_pushcfunction(L, LuaMotionGc);
    lua_setfield(L, -2, "__gc");

    // Register methods into metatable
    luaL_register(L, NULL, Meta_methods);

    // Pop metatable
    lua_pop(L, 1);

    // Register module-level functions as "motionhelper" global
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
}
