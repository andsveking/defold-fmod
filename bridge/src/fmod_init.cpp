#include "fmod_bridge.hpp"
#include <fmod_errors.h>
#include <LuaBridge/LuaBridge.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace FMODBridge;
using namespace luabridge;

FMOD_STUDIO_SYSTEM* FMODBridge::system = NULL;
FMOD_SYSTEM* FMODBridge::lowLevelSystem = NULL;
bool FMODBridge::isPaused = false;

#ifdef FMOD_BRIDGE_LOAD_DYNAMICALLY
bool FMODBridge::isLinked = false;
#endif

static FMOD_SPEAKERMODE speakerModeFromString(const char* str) {
    if (0 == strcmp(str, "default")) { return FMOD_SPEAKERMODE_DEFAULT; }
    if (0 == strcmp(str, "stereo")) { return FMOD_SPEAKERMODE_STEREO; }
    if (0 == strcmp(str, "mono")) { return FMOD_SPEAKERMODE_MONO; }
    if (0 == strcmp(str, "5.1")) { return FMOD_SPEAKERMODE_5POINT1; }
    if (0 == strcmp(str, "7.1")) { return FMOD_SPEAKERMODE_7POINT1; }
    if (0 == strcmp(str, "quad")) { return FMOD_SPEAKERMODE_QUAD; }
    if (0 == strcmp(str, "surround")) { return FMOD_SPEAKERMODE_SURROUND; }
    if (0 == strcmp(str, "max")) { return FMOD_SPEAKERMODE_MAX; }
    if (0 == strcmp(str, "raw")) { return FMOD_SPEAKERMODE_RAW; }
    LOGW("Invalid value for speaker_mode: \"%s\". Using default", str);
    return FMOD_SPEAKERMODE_DEFAULT;
}

#define check(fcall) do { \
    FMOD_RESULT res = fcall; \
    if (res != FMOD_OK) { \
        LOGE("%s", FMOD_ErrorString(res)); \
        FMOD_Studio_System_Release(FMODBridge::system); \
        FMODBridge::system = NULL; \
        return; \
    } \
} while(0)

extern "C" void FMODBridge_init(lua_State *L) {
    #ifdef __EMSCRIPTEN__
    EM_ASM(Module.cwrap = Module.cwrap || cwrap);
    #endif

    #ifdef FMOD_BRIDGE_LOAD_DYNAMICALLY
    isLinked = linkLibraries();
    if (!isLinked) {
        LOGW("FMOD libraries could not be loaded. FMOD will be disabled for this session");
        return;
    }
    #endif

    ensure(ST, FMOD_Studio_System_Create, FMOD_RESULT, FMOD_STUDIO_SYSTEM**, unsigned int);
    ensure(ST, FMOD_Studio_System_GetLowLevelSystem, FMOD_RESULT, FMOD_STUDIO_SYSTEM*, FMOD_SYSTEM**);
    ensure(LL, FMOD_System_SetSoftwareFormat, FMOD_RESULT, FMOD_SYSTEM*, int, FMOD_SPEAKERMODE, int);
    ensure(LL, FMOD_System_SetDSPBufferSize, FMOD_RESULT, FMOD_SYSTEM*, unsigned int, int);
    ensure(ST, FMOD_Studio_System_Initialize, FMOD_RESULT, FMOD_STUDIO_SYSTEM*, int, FMOD_STUDIO_INITFLAGS, FMOD_INITFLAGS, void*);
    ensure(ST, FMOD_Studio_System_Release, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);

    FMOD_RESULT res;
    res = FMOD_Studio_System_Create(&FMODBridge::system, FMOD_VERSION);
    if (res != FMOD_OK) {
        LOGE("%s", FMOD_ErrorString(res));
        FMODBridge::system = NULL;
        return;
    }

    check(FMOD_Studio_System_GetLowLevelSystem(FMODBridge::system, &lowLevelSystem));

    int defaultSampleRate = 0;
    #ifdef __EMSCRIPTEN__
    check(FMOD_System_GetDriverInfo(lowLevelSystem, 0, NULL, 0, NULL, &defaultSampleRate, NULL, NULL));
    check(FMOD_System_SetDSPBufferSize(lowLevelSystem, 2048, 2));
    #endif

    int sampleRate = FMODBridge_dmConfigFile_GetInt("fmod.sample_rate", defaultSampleRate);
    int numRawSpeakers = FMODBridge_dmConfigFile_GetInt("fmod.num_raw_speakers", 0);
    const char* speakerModeStr = FMODBridge_dmConfigFile_GetString("fmod.speaker_mode", "default");
    FMOD_SPEAKERMODE speakerMode = speakerModeFromString(speakerModeStr);

    if (sampleRate || numRawSpeakers || speakerMode != FMOD_SPEAKERMODE_DEFAULT) {
        check(FMOD_System_SetSoftwareFormat(lowLevelSystem, sampleRate, speakerMode, numRawSpeakers));
    }

    FMOD_STUDIO_INITFLAGS studioInitFlags = FMOD_STUDIO_INIT_NORMAL;
    if (FMODBridge_dmConfigFile_GetInt("fmod.live_update", 0)) {
        studioInitFlags |= FMOD_STUDIO_INIT_LIVEUPDATE;
    }

    void* extraDriverData = NULL;
    check(FMOD_Studio_System_Initialize(FMODBridge::system, 1024, studioInitFlags, FMOD_INIT_NORMAL, extraDriverData));

    isPaused = false;

    #ifdef __EMSCRIPTEN__
    EM_ASM({
        Module._FMODBridge_onClick = function () {
            ccall('FMODBridge_unmuteAfterUserInteraction', null, [], []);
            Module.canvas.removeEventListener('click', Module._FMODBridge_onClick);
        };
        Module.canvas.addEventListener('click', Module._FMODBridge_onClick, false);
    });
    #endif

    #if TARGET_OS_IPHONE
    FMODBridge::initIOSInterruptionHandler();
    #endif

    registerEnums(L);
    registerClasses(L);
}

extern "C" void FMODBridge_update() {
    if (!FMODBridge::system || FMODBridge::isPaused) { return; }

    ensure(ST, FMOD_Studio_System_Update, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);
    ensure(ST, FMOD_Studio_System_Release, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);

    FMOD_RESULT res = FMOD_Studio_System_Update(FMODBridge::system);
    if (res != FMOD_OK) {
        LOGE("%s", FMOD_ErrorString(res));
        FMOD_Studio_System_Release(FMODBridge::system);
        FMODBridge::system = NULL;
        return;
    }
}

extern "C" void FMODBridge_finalize() {
    if (FMODBridge::system) {
        ensure(ST, FMOD_Studio_System_Release, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);

        FMOD_RESULT res = FMOD_Studio_System_Release(FMODBridge::system);
        if (res != FMOD_OK) { LOGE("%s", FMOD_ErrorString(res)); }
        FMODBridge::system = NULL;
    }

    #ifdef __EMSCRIPTEN__
    EM_ASM({
        Module.canvas.removeEventListener('click', Module._FMODBridge_onClick);
    });
    #endif

    #ifdef FMOD_BRIDGE_LOAD_DYNAMICALLY
    if (isLinked) { cleanupLibraries(); }
    #endif
}

void FMODBridge::resumeMixer() {
    if (FMODBridge::system && FMODBridge::isPaused) {
        ensure(ST, FMOD_Studio_System_Release, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);
        ensure(LL, FMOD_System_MixerResume, FMOD_RESULT, FMOD_SYSTEM*);
        check(FMOD_System_MixerResume(FMODBridge::lowLevelSystem));
        FMODBridge::isPaused = false;
    }
}

void FMODBridge::suspendMixer() {
    if (FMODBridge::system && !FMODBridge::isPaused) {
        ensure(ST, FMOD_Studio_System_Release, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);
        ensure(LL, FMOD_System_MixerSuspend, FMOD_RESULT, FMOD_SYSTEM*);
        check(FMOD_System_MixerSuspend(FMODBridge::lowLevelSystem));
        FMODBridge::isPaused = true;
    }
}

#ifdef __EMSCRIPTEN__
__attribute__((used))
extern "C" void FMODBridge_unmuteAfterUserInteraction() {
    if (FMODBridge::system && !FMODBridge::isPaused) {
        ensure(ST, FMOD_Studio_System_Release, FMOD_RESULT, FMOD_STUDIO_SYSTEM*);
        ensure(LL, FMOD_System_MixerSuspend, FMOD_RESULT, FMOD_SYSTEM*);
        ensure(LL, FMOD_System_MixerResume, FMOD_RESULT, FMOD_SYSTEM*);
        check(FMOD_System_MixerSuspend(FMODBridge::lowLevelSystem));
        check(FMOD_System_MixerResume(FMODBridge::lowLevelSystem));
    }
}
#endif

extern "C" void FMODBridge_activateApp() {
    #if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    FMODBridge::resumeMixer();
    #endif
}

extern "C" void FMODBridge_deactivateApp() {
    #if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    FMODBridge::suspendMixer();
    #endif
}
