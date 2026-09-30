#pragma once

#include "../CompilerTypes.hpp"
#include "ASTNodes.hpp"
#include "ASTParser.hpp"
#include "ArenaAllocator.hpp"
#include "Diagnostics.hpp"
#include "Lexer.hpp"
#include "Libraries/include/enkits/enkiTS/TaskScheduler.h"
#include "Libraries/include/unordered_dense/ankerl/unordered_dense.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/*
 * INFO:
 * circular and duplicates are instantly fatalized; too much work to support bad architecture
 *
 * Idea is:
 * We assign each thread its own context, and let each thread allocate tasks freely (for work
 * stealing)
 *
 * Each thread loads files into a thread local hashmap
 * Then each single thread is assigned to its own file
 * The thread parses its file, pushes the filepath to a vector as a string_view
 * and then adds it as a task to the task scheduler
 *
 * Then repeat until hitting a root node
 *
 * When hitting a root node, the thread checks the metadata to ensure that the file mtime and
 * compiler versions are different than documented time, then if it is, the thread writes to its
 * leaf node hashmap
 *
 * Repeat until task completion
 *
 * Move onto second pass
 */

namespace JSlang
{
struct DependencyGraphResolver
{
    enki::TaskScheduler TaskScheduler;
    DiagnosticEngine    ObjectDiagnosticEngine;

    bool StoreCacheToDisk = true;

    DependencyGraphResolver(uint32_t InitWithThreadNum)
    {
        TaskScheduler.Initialize(InitWithThreadNum);
        ObjectDiagnosticEngine.TaskScheduler = &TaskScheduler;
    }

    using CompiledHeaders = ankerl::unordered_dense::map<std::string_view, AST::Parser::Module>;

    struct Node
    {
        std::string_view  Name;
        std::span<Node *> Edges;
    };

    struct ThreadLocalContext
    {
        using ModuleSourceHashmap =
            ankerl::unordered_dense::map<std::string_view, std::string_view>;
        using LeafNodeHashmap = ankerl::unordered_dense::map<std::string_view, Node>;

        std::vector<std::string_view> Filepaths; // ring buffer that gets larger with more threads,
                                                 // smaller with less; controllable by user
        size_t
            CurrentTaskStartPointer; // so the vector never needs to have one of its members popped

        ArenaAllocator ObjectArenaAllocator;

        LeafNodeHashmap     LeafNodes;
        ModuleSourceHashmap ModuleSource;
    };

    uint8_t *GenerateCodeFor(
        CompilerTargets  CompilerTarget,
        std::string_view Source,
        std::string_view SourceFilepath)
    {
        ArenaAllocator ObjectArenaAllocator;

        Lexer       lexer(ObjectDiagnosticEngine, Source, SourceFilepath);
        AST::Parser parser(lexer, ObjectArenaAllocator);

        std::vector<std::string_view> dependencies;
        auto                         *import_statements = parser.ParseImportStatements();

        for (auto *ImportStatement : import_statements->TopLevelNodes)
        {
            auto *cast_import_statement = static_cast<AST::ImportStatement *>(ImportStatement);

            // already reported about in the untyped parser
            if (cast_import_statement->ImportFrom.empty())
            {
                continue;
            }

            dependencies.push_back(cast_import_statement->ImportFrom);
        }


    }
};
} // namespace JSlang
