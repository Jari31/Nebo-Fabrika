#pragma once

namespace JSlang
{
struct MacroEngineCallbacks
{
    void (*StaticAnalysis)(unsigned) = nullptr;
};
} // namespace JSlang
