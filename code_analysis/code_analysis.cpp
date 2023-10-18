#include "code_analysis.h"

clang::VarDecl* getVariableDeclaration(clang::DeclRefExpr* DRE){
  if(clang::VarDecl* Variable = clang::dyn_cast<clang::VarDecl>(DRE->getDecl()))
    return Variable;
  else
    return nullptr;
}


clang::DeclRefExpr* getVariableReference(clang::Stmt* S){
  if(clang::DeclRefExpr* VariableReference = clang::dyn_cast<clang::DeclRefExpr>(S))
    return VariableReference;
  
  for(auto& Child : S->children()){
    if(clang::DeclRefExpr* VariableReference = clang::dyn_cast<clang::DeclRefExpr>(Child))
      return VariableReference;
    else
      return getVariableReference(Child);
  }
  
  return nullptr;
}


clang::BinaryOperator* getVariableDefinition(clang::VarDecl* Decl, std::map<clang::VarDecl*, clang::BinaryOperator*> Definitions){
  for(const auto& [Var, Def] : Definitions){
    clang::VarDecl* AssignedVar = getVariableDeclaration(getVariableReference(Def->getLHS()));
    if(AssignedVar == Decl)
      return Def;
  }

  return nullptr;
}


bool isParameter(clang::ValueDecl* Decl){
  return clang::isa<clang::ParmVarDecl>(Decl) ? true : false;
}


bool isReturn(clang::ValueDecl* Var, clang::Expr* ReturnValue){
  if(ReturnValue)
    return getVariableDeclaration(getVariableReference(ReturnValue)) == Var ? true : false;
  
  return false;
}


bool isTACOSupportedExpr(clang::Expr* E){
  if(clang::BinaryOperator* BO = clang::dyn_cast<clang::BinaryOperator>(E)){
    switch (BO->getOpcode()){
      case clang::BinaryOperator::Opcode::BO_Add:
      case clang::BinaryOperator::Opcode::BO_Sub:
      case clang::BinaryOperator::Opcode::BO_Mul:
      case clang::BinaryOperator::Opcode::BO_Div:
        return true;
    
      default:
        return false;
    }
  }
  return false;
}


bool mayBeOutputAlias(clang::VarDecl* Decl, clang::BinaryOperator* Definition){
  if(!Decl->getType()->isPointerType())
    return false;

  // If this is a pointer dereference, it may be an alias to the output variable
  clang::Stmt* FirstChild = *(Definition->child_begin());
  if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(FirstChild))
    return true;
  
  if(Decl->hasInit()){
    clang::VarDecl* VarUsedInDecl = getVariableDeclaration(getVariableReference(Decl->getInit()));
    if(VarUsedInDecl)
      if(isParameter(VarUsedInDecl))
        return isTACOSupportedExpr(Definition->getRHS());

  }

  return false;
}


bool multiplicationInDefinition(clang::Stmt* S, std::map<clang::VarDecl*, clang::BinaryOperator*> Definitions){
  if(clang::BinaryOperator* OP = clang::dyn_cast<clang::BinaryOperator>(S)){
    if(OP->isMultiplicativeOp())
      return true;
  }

  // We do not consider multiplication in array index expressions
  if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S))
    S = ASE->getBase();
  
  if(clang::DeclRefExpr* DRE = clang::dyn_cast<clang::DeclRefExpr>(S)){
    clang::VarDecl* Decl = getVariableDeclaration(getVariableReference(DRE));
    if(Decl->isLocalVarDecl()){
      clang::BinaryOperator* LocalVarDef = getVariableDefinition(Decl, Definitions);
      if(LocalVarDef)
        return multiplicationInDefinition(LocalVarDef->getRHS(), Definitions);
        
    }
  }
  
  for(auto& Child : S->children())
    return multiplicationInDefinition(Child, Definitions);

  return false;
}


bool mayBeMM(clang::BinaryOperator* Definition, std::map<clang::VarDecl*, clang::BinaryOperator*> Definitions){
  // We heuristically assume that when the output array is indexed with a linear expression
  // that corresponds to a matrix multiplication kernel, so we do not add the opcode
  // of this definition even though this is an compound assignment
  if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(Definition->getLHS())){
    clang::Expr* RHS = ASE->getIdx();
    if(clang::isa<clang::BinaryOperator>(*(RHS->child_begin()))){
      return multiplicationInDefinition(Definition->getRHS(), Definitions);
    }
  }
  else if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(Definition->getLHS())){
    if(UO->getOpcode() == clang::UnaryOperator::Opcode::UO_Deref){
      // Array manipulation through pointers 
      return multiplicationInDefinition(UO, Definitions);
    }
  }

  return false;
}