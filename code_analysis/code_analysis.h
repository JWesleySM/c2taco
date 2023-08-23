#ifndef CODE_ANALYSIS_H
#define CODE_ANALYSIS_H

#include "clang/Frontend/CompilerInstance.h"

clang::VarDecl* getVariableDeclaration(clang::DeclRefExpr* DRE);

clang::DeclRefExpr* getVariableReference(clang::Stmt* S);

clang::BinaryOperator* getVariableDefinition(clang::VarDecl* Var, std::map<clang::VarDecl*, clang::BinaryOperator*> Definitions);

bool isParameter(clang::ValueDecl* Decl);

bool isReturn(clang::ValueDecl* Var, clang::Expr* ReturnValue);

bool mayBeOutputAlias(clang::VarDecl* Decl, clang::BinaryOperator* Definition);

bool multiplicationInDefinition(clang::Stmt* S, std::map<clang::VarDecl*, clang::BinaryOperator*> Definitions);

bool mayBeMM(clang::BinaryOperator* Definition, std::map<clang::VarDecl*, clang::BinaryOperator*> Definitions);

#endif