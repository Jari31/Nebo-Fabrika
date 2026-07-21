#include "Libraries/include/slang.h"
#include <Libraries/include/slang-com-ptr.h>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace ShaderCompiler
{
namespace ShaderSlang
{
struct CompileToSPIRV_Options
{
    const char *Source         = "";
    const char *EntryPointName = "";
    const char *ShaderName     = "Undefined";
    const char *PathToShader   = "Undefined";
};

template <size_t SearchPathArraySize>
auto CompileSourceToSPIRV(
    CompileToSPIRV_Options                        &Options,
    const std::array<char *, SearchPathArraySize> &SearchPaths) -> std::vector<uint8_t> // NOLINT
{
    Slang::ComPtr<slang::IGlobalSession> GlobalSession;
    slang::createGlobalSession(GlobalSession.writeRef());

    slang::SessionDesc session_description = {};
    slang::TargetDesc  target_description  = {};
    target_description.format              = SLANG_SPIRV;
    target_description.profile             = GlobalSession->findProfile("spirv_1_5");

    session_description.targets     = &target_description;
    session_description.targetCount = 1;
    if (std::size(SearchPaths) > 0)
    {
        session_description.searchPaths     = SearchPaths.data();
        session_description.searchPathCount = std::size(SearchPaths);
    }
    Slang::ComPtr<slang::ISession> session;
    GlobalSession->createSession(session_description, session.writeRef());

    Slang::ComPtr<slang::IBlob> diagnostics_blob;
    slang::IModule             *module = session->loadModuleFromSourceString(
        Options.EntryPointName, Options.PathToShader, Options.Source, diagnostics_blob.writeRef());

    if (module == nullptr)
    {
        return {};
    }

    Slang::ComPtr<slang::IEntryPoint> entry_point;
    module->findEntryPointByName(Options.EntryPointName, entry_point.writeRef());

    slang::IComponentType               *components[] = {module, entry_point}; // NOLINT
    Slang::ComPtr<slang::IComponentType> program;
    session->createCompositeComponentType(components, 2, program.writeRef());

    Slang::ComPtr<slang::IBlob> spirv_code;
    program->getEntryPointCode(0, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());

    const auto *buffer      = static_cast<const uint8_t *>(spirv_code->getBufferPointer());
    size_t      buffer_size = spirv_code->getBufferSize();

    return std::vector<uint8_t>(buffer, buffer + buffer_size);
}
}; // namespace ShaderSlang
} // namespace ShaderCompiler
