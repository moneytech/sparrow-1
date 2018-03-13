#pragma once

#include "Nest/Api/Location.h"

#include "LlvmBuilder.h"

#include <llvm/IR/DIBuilder.h>

typedef struct Nest_Node Node;
typedef struct Nest_SourceCode SourceCode;

namespace LLVMB {
namespace Tr {
/// Helper class used to generate debug information for the translated compile unit.
///
/// This is based on the functionality from the CLang compiler.
class DebugInfo {
public:
    DebugInfo(llvm::Module& module, const string& mainFilename);
    ~DebugInfo();

    /// Finalize the generation of debug information for this program
    void finalize();

    void emitLocation(LlvmBuilder& builder, const Location& loc, bool takeStart = true);

    void emitFunctionStart(LlvmBuilder& builder, Node* fun, llvm::Function* llvmFun);
    void emitFunctionEnd(LlvmBuilder& builder, const Location& loc);

    void emitLexicalBlockStart(LlvmBuilder& builder, const Location& loc);
    void emitLexicalBlockEnd(LlvmBuilder& builder, const Location& loc);

private:
    void createCompileUnit(const string& mainFilename);

    void setLocation(LlvmBuilder& builder, const Location& loc);

    llvm::DIFile* getOrCreateFile(const Location& loc);

    llvm::DISubroutineType* createFunctionType(llvm::Function* llvmFun);

private:
    /// The object used to build the debug information for the generated program
    llvm::DIBuilder diBuilder_;

    /// Debug information for the compile-unit - this corresponds to the whole program
    llvm::DICompileUnit* compileUnit_;

    /// Stack of current nested lexical blocks
    vector<llvm::DIScope*> lexicalBlockStack_;

    /// Stack of lexical block region count at start of each function
    /// Starting a function will push an element, ending it will pop an element
    vector<unsigned> regionCountAtFunStartStack_;

    /// Map from declarations to the corresponding metadata nodes
    llvm::DenseMap<const Node*, llvm::DINode*> regionMap_;

    /// Map containing the filename nodes - the filenames are represented by the SourceCode pointer
    llvm::DenseMap<const SourceCode*, llvm::DIFile*> filenameCache_;

    /// The current location for which we set debug information
    Location curLoc_;
    /// The location that we previously written debug information for
    Location prevLoc_;
};
} // namespace Tr
} // namespace LLVMB
