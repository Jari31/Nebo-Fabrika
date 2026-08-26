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

    void CompileFromSource(JSlang::CompileFromSourceRequest CompileRequest) override
    {
        ObjectCompiler.CompileFromSource(CompileRequest);
    };
};
} // namespace JSlang
extern "C"
{
    DLL_EXPORT IJSlang *CreateJSlangInterfaceInstance()
    {
        return new JSlang::PluginJSlang();
    }

    DLL_EXPORT void DestroyJSlangInterfaceInstance(IJSlang *IJSlangInstance)
    {
        delete IJSlangInstance;
    }
}
