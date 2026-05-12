#define EXTENSION_NAME SDLInput
#define LIB_NAME "SDLInput"
#define MODULE_NAME "sdlinput"

#include <dmsdk/sdk.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_error.h>

#include <sdlinput_bindings.h>
#include <motionhelper_bindings.h>

static dmExtension::Result AppInitializeSDLInput(dmExtension::AppParams* params)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        dmLogError("SDLInput: SDL_Init failed: %s", SDL_GetError());
    }

    // continue even if init fails
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeSDLInput(dmExtension::Params* params)
{
    sdlinput_register(params->m_L);
    motionhelper_register(params->m_L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeSDLInput(dmExtension::Params* params)
{
    SDL_Quit();
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeSDLInput, 0, InitializeSDLInput, 0, 0, FinalizeSDLInput)
