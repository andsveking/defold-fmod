#define LIB_NAME "DefoldFMOD"
#define MODULE_NAME "fmod"

#if defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_WINDOWS) || defined(DM_PLATFORM_LINUX)

#include "fmod_bridge_interface.hpp"

dmExtension::Result InitializeDefoldFMOD(dmExtension::Params* params) {
    FMODBridge_init(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateDefoldFMOD(dmExtension::Params* params) {
    FMODBridge_update();
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDefoldFMOD(dmExtension::Params* params) {
    FMODBridge_finalize();
    return dmExtension::RESULT_OK;
}

int FMODBridge_dmBuffer_GetBytes(FMODBridge_HBuffer buffer, void** bytes, uint32_t* size) {
    return dmBuffer::GetBytes((dmBuffer::HBuffer)buffer, bytes, size) != dmBuffer::RESULT_OK;
}

void FMODBridge_dmScript_PushBuffer(lua_State* L, FMODBridge_HBuffer buffer) {
    dmScript::LuaHBuffer wrapper = { buffer, false };
    dmScript::PushBuffer(L, wrapper);
}

FMODBridge_HBuffer FMODBridge_dmScript_CheckBuffer(lua_State* L, int index) {
    return dmScript::CheckBuffer(L, index)->m_Buffer;
}

#else

dmExtension::Result InitializeDefoldFMOD(dmExtension::Params* params) {
    return dmExtension::RESULT_OK;
}

#define UpdateDefoldFMOD 0

dmExtension::Result FinalizeDefoldFMOD(dmExtension::Params* params) {
    return dmExtension::RESULT_OK;
}

#endif

dmExtension::Result AppInitializeDefoldFMOD(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDefoldFMOD(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(DefoldFMOD, LIB_NAME, AppInitializeDefoldFMOD, AppFinalizeDefoldFMOD, InitializeDefoldFMOD, UpdateDefoldFMOD, 0, FinalizeDefoldFMOD)

// Stub extension so the user can link to 3rd party libs

dmExtension::Result InitializeDefoldFMODStub(dmExtension::Params* params) {
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDefoldFMODStub(dmExtension::Params* params) {
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppInitializeDefoldFMODStub(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeDefoldFMODStub(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(DefoldFMODStub, "fmodstub", AppInitializeDefoldFMODStub, AppFinalizeDefoldFMODStub, InitializeDefoldFMODStub, 0, 0, FinalizeDefoldFMODStub)
