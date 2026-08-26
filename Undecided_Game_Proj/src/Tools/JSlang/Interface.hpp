#pragma once

#include "CompilerTypes.hpp"
class IJSlang
{
  public:
    virtual ~IJSlang()                                                              = default;
    virtual void Initialize(JSlang::CompilerInitializationOptions Options)          = 0;
    virtual void CompileFromSource(JSlang::CompileFromSourceRequest CompileRequest) = 0;
};
