#pragma once

#include <cstdint>
namespace JSlang
{
enum class ErrorCodes : uint32_t // NOLINT
{
    UNKNOWN_ERROR,
};

struct ErrorCode
{
    ErrorCodes  Code    = ErrorCodes::UNKNOWN_ERROR;
    const char *Message = nullptr;
    uint32_t    Line    = 0;
    uint32_t    Column  = 0;
};

enum class CompilerTargets : uint32_t // NOLINT
{
    IR           = 0,
    TOKENS       = 1 << 0,
    AST          = 1 << 1,
    SLANG_SOURCE = 1 << 2,
    CPP_SOURCE   = 1 << 3,
    ISPC_SOURCE  = 1 << 4,
};

struct CompileFromSourceRequest
{
    const char           *SourceCode     = nullptr;
    const char           *SourceFileName = nullptr;
    const CompilerTargets Targets        = CompilerTargets::AST;
    const uint32_t        Optimization   = 0;
};

struct CompileResult
{
    ErrorCode     *Error                        = nullptr;
    uint32_t       SizeOfGeneratedSourceInBytes = 0;
    const uint8_t *GeneratedSource              = nullptr;
};

struct CompilerInitializationOptions
{
    uint32_t CompileWithThreads = 1;
    bool     Verbose            = false;
};
} // namespace JSlang
