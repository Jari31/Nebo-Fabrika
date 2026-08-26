#include "Compiler.hpp"
#include "CompilerTypes.hpp"
#include "Interface.hpp"

#if defined(_WIN32)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __attribute__((visibility("default")))
#endif

namespace JSlang
{
class PluginJSlang : public IJSlang
{
  private:
    Compiler ObjectCompiler;

  public:
    void Initialize(CompilerInitializationOptions Options) override
    {
        ObjectCompiler.Initialize(Options);
    }

    CompileResult CompileFromSource(CompileFromSourceRequest CompileRequest) override
    {
        return ObjectCompiler.CompileFromSource(CompileRequest);
    }
};
} // namespace JSlang
extern "C"
{
    DLL_EXPORT JSlang::IJSlang *CreateJSlangInterfaceInstance()
    {
        return new JSlang::PluginJSlang();
    }

    DLL_EXPORT void DestroyJSlangInterfaceInstance(JSlang::IJSlang *IJSlangInstance)
    {
        delete IJSlangInstance;
    }
}
