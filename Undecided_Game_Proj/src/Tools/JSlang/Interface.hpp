#pragma once

#include "CompilerTypes.hpp"
namespace JSlang
{
class IJSlang
{
  public:
    virtual ~IJSlang()                                                               = default;
    virtual void          Initialize(CompilerInitializationOptions Options)          = 0;
    virtual CompileResult CompileFromSource(CompileFromSourceRequest CompileRequest) = 0;
};
} // namespace JSlang
