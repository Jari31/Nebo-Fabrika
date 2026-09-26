#pragma once

#ifdef ASTNODE_FFI
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long long size_t;

typedef struct
{
    const char *Data;
    size_t      Size;
} string_view;

typedef struct
{
    void **Data;
    size_t Size;
} ASTNodeSpan;

#define std_string_view string_view
#define std_span_ASTNode_ptr ASTNodeSpan

#include "ASTNodes.hpp"

#else

#include <span>
#include <string_view>

namespace JSlang
{
using string_view = std::string_view;
#ifndef ASTNODE

#else
using ASTNodeSpan = std::span<ASTNode *>;
#endif
} // namespace JSlang

#endif
