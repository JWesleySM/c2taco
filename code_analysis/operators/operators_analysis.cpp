//==============================================================================
// FILE:
//    operator_analysis.cpp
//
// AUTHOR:
//    José Wesley De Souza Magalhães [jwesley.magalhaes@ed.ac.uk | jwdesmagalhaes@gmail.com]
//
// DESCRIPTION:
//    Perform a static analysis to predict which binary operators are likely to be in 
//    the target TACO program.                                                                                
//
// USAGE:
//   clang -cc1 -load $BUILD_DIR/libOperatorAnalysis.so '\'
//    -plugin operator-analysis program.c
// OR:
//   clang -c -Xclang -load -Xclang $BUILD_DIR/libOperatorAnalysis.so  '\'
//    -Xclang -plugin -Xclang operator-analysis program.c
//
// Where $BUILD_DIR points to the directory where you built this library
//
//==============================================================================

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Rewrite/Core/Rewriter.h"

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


//-----------------------------------------------------------------------------
// RecursiveASTVisitor
//-----------------------------------------------------------------------------
class OperatorAnalysisVisitor : public clang::RecursiveASTVisitor<OperatorAnalysisVisitor>{
    
public:
  
  explicit OperatorAnalysisVisitor(clang::ASTContext* context)
           :Rewriter_(clang::Rewriter(context->getSourceManager(), context->getLangOpts())){}

  clang::Rewriter Rewriter_;
  std::map<clang::VarDecl*, clang::BinaryOperator*>Definitions_;
  clang::Expr* ReturnValue_ = nullptr;


  bool VisitVarDecl(clang::VarDecl *VD){
    if(!VD->isLocalVarDecl())
      return true;
    
    if(!VD->hasInit())
      return true;

    clang::DeclRefExpr* VarReference = clang::DeclRefExpr::CreateEmpty(VD->getASTContext(), false, true, false, 0);
    VarReference->setType(VD->getType());
    VarReference->setDecl(VD);
    clang::BinaryOperator* VarInit = clang::BinaryOperator::CreateEmpty(VD->getASTContext(), false);
    VarInit->setLHS(VarReference);
    VarInit->setRHS(VD->getInit());
    VarInit->setOpcode(clang::BinaryOperator::Opcode::BO_Assign);
    Definitions_[VD] = VarInit;
    return true;  
  }

  bool VisitBinaryOperator(clang::BinaryOperator* BO){
    if(!BO->isAssignmentOp())
      return true;
    
    Definitions_[getVariableDeclaration(getVariableReference(BO->getLHS()))] = BO;
    return true;
  }


  bool VisitReturnStmt(clang::ReturnStmt* RS){
    if(RS->getRetValue()->getType()->isIntegerType()){
      if(!clang::isa<clang::IntegerLiteral>(RS->getRetValue()))
        ReturnValue_ = RS->getRetValue();
    } 

    return true;
  }

};


//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class OperatorAnalysisConsumer : public clang::ASTConsumer{
private:
  OperatorAnalysisVisitor* Visitor_;

  
  bool isParameter(clang::ValueDecl* Decl){
    return clang::isa<clang::ParmVarDecl>(Decl) ? true : false;
  }


  bool isReturn(clang::ValueDecl* Decl){
    if(Visitor_->ReturnValue_){
      return getVariableDeclaration(getVariableReference(Visitor_->ReturnValue_)) == Decl ? true : false;
    }

    return false;
  }


  bool mayBeOutputAlias(clang::VarDecl* Decl, clang::BinaryOperator* Definition){
    if(!Decl->getType()->isPointerType())
      return false;
  
    // If this is a pointer dereference, it may be an alias to the output variable
    clang::Stmt* FirstChild = *(Definition->child_begin());
    if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(FirstChild)){
      return true;
    }

    return false;

  }

  bool multiplicationInDefinition(clang::Stmt* S){
    if(clang::BinaryOperator* OP = clang::dyn_cast<clang::BinaryOperator>(S)){
      if(OP->isMultiplicativeOp())
        return true;
    }
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S)){
      S = ASE->getBase();
    }

    if(clang::DeclRefExpr* DRE = clang::dyn_cast<clang::DeclRefExpr>(S)){
      clang::VarDecl* Decl = getVariableDeclaration(getVariableReference(DRE));
      if(Decl->isLocalVarDecl()){
        for(const auto& [Var, Def] : Visitor_->Definitions_){
          clang::VarDecl* AssignedVar = getVariableDeclaration(getVariableReference(Def->getLHS()));
          if(AssignedVar == Decl){
             return multiplicationInDefinition(Def->getRHS());
          }
        }
      }
    }
    
    for(auto& Child : S->children())
      return multiplicationInDefinition(Child);

    return false;

  }


  bool mayBeMM(clang::BinaryOperator* Definition){
    // We heuristically assume that when the output array is indexed with a linear expression
    // that corresponds to a matrix multiplication kernel, so we do not add the opcode
    // of this definition even though this is an compound assignment
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(Definition->getLHS())){
      clang::Expr* RHS = ASE->getIdx();
      if(clang::isa<clang::BinaryOperator>(*(RHS->child_begin()))){
        return multiplicationInDefinition(Definition->getRHS());
      }
    }
    else if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(Definition->getLHS())){
      if(UO->getOpcode() == clang::UnaryOperator::Opcode::UO_Deref){
        // Array manipulation through pointers 
        return multiplicationInDefinition(UO);
      }
    }

    return false;
  }


  std::string getOperatorOpcode(clang::BinaryOperator* BO){
    switch(BO->getOpcode()){
      case clang::BinaryOperator::Opcode::BO_Add:
      case clang::BinaryOperator::Opcode::BO_AddAssign:
        return "+";
      case clang::BinaryOperator::Opcode::BO_Sub:
      case clang::BinaryOperator::Opcode::BO_SubAssign:
        return "-";
      case clang::BinaryOperator::Opcode::BO_Mul:
      case clang::BinaryOperator::Opcode::BO_MulAssign:
        return "*";
      case clang::BinaryOperator::Opcode::BO_Div:
      case clang::BinaryOperator::Opcode::BO_DivAssign:
        return "/";
      
      default:
        return "";
    }
  }


  void getOperatorsInDefinition(clang::Stmt* S, std::set<clang::Stmt*>* Operators){
    if(clang::isa<clang::BinaryOperator>(S)){
        Operators->insert(S);
    }

    if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(S)){
      // Unary minus 
      if(UO->getOpcode() == clang::UnaryOperator::Opcode::UO_Minus){
        Operators->insert(UO);
        return;
      }
    }

    // Do not add variables used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S)){
      return;
    }

    for(auto& Child : S->children())
      getOperatorsInDefinition(Child, Operators);

  }


  void getVariablesInDefinition(clang::Stmt* S, std::vector<clang::VarDecl*>* Vars){
    if(clang::DeclRefExpr* Reference = clang::dyn_cast<clang::DeclRefExpr>(S)){
      clang::VarDecl* Variable = getVariableDeclaration(Reference);
      if(Variable)
        Vars->push_back(Variable);
    }

    // Do not add variables used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S)){
      S = ASE->getBase();
    }

    for(auto& Child : S->children())
      getVariablesInDefinition(Child, Vars);
      
  }

  void getOperatorsInLocalVarDefinition(clang::VarDecl* LocalVar, std::set<clang::Stmt*>*Operators){
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      clang::VarDecl* AssignedVar = getVariableDeclaration(getVariableReference(Def->getLHS()));
      if(AssignedVar == LocalVar){
        getOperatorsInDefinition(Def->getRHS(), Operators);
        return;
      }
    }
  }


public:

  explicit OperatorAnalysisConsumer(clang::CompilerInstance *CI)
         :Visitor_(new OperatorAnalysisVisitor(&CI->getASTContext())){}


  std::set<std::string> AnalyseDefinitions(){
    std::set<std::string> Operators;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      //llvm::outs() << "Defintion: " << Visitor_->Rewriter_.getRewrittenText(Def->getSourceRange()) << "\n";
      clang::DeclRefExpr* Definition = getVariableReference(Def->getLHS());
      if(!Definition){
        llvm::errs() << "Could not get definition refered to in the assignment: " << Visitor_->Rewriter_.getRewrittenText(Def->getSourceRange()) << "\n";
        return Operators;
      }
      // We found an assignment to the output variable
      if(isParameter(Var) || isReturn(Var) || mayBeOutputAlias(Var, Def)){
        //llvm::outs() << "We need to analyze this\n";
        std::set<clang::Stmt*>OperatorsInDef;
        getOperatorsInDefinition(Def->getRHS(), &OperatorsInDef);
        for(auto& Op : OperatorsInDef){
          if(clang::BinaryOperator* BO = clang::dyn_cast<clang::BinaryOperator>(Op)){
            Operators.insert(getOperatorOpcode(BO));
          }
        }

        // Operators in the definition of local variables
        std::vector<clang::VarDecl*> Vars;
        getVariablesInDefinition(Def->getRHS(), &Vars);
        for(auto& V : Vars){
          if(V->isLocalVarDecl()){
            std::set<clang::Stmt*>OperatorsInLocalVarDef;
            getOperatorsInLocalVarDefinition(V, &OperatorsInLocalVarDef);
            for(auto& Op : OperatorsInLocalVarDef){
              if(clang::BinaryOperator* BO = clang::dyn_cast<clang::BinaryOperator>(Op)){
                Operators.insert(getOperatorOpcode(BO));
              }
            }
          }
        }

        // Operators resulting from compound assignments
        if(Def->isCompoundAssignmentOp()){
          if(Visitor_->ReturnValue_)
            continue;
          if(mayBeMM(Def))
            continue;

          std::string CompoundOp = getOperatorOpcode(Def);
          Operators.insert(CompoundOp);
        }
      }
      else{
        //llvm::outs() << "We should not analyze this\n";
      }
    }
    return Operators;
  } 


  void HandleTranslationUnit(clang::ASTContext &Context) override{
    Visitor_->TraverseDecl(Context.getTranslationUnitDecl());
    for(std::string Op : AnalyseDefinitions())
      llvm::outs() << Op <<" ";
    llvm::outs() <<"\n";
    ////llvm::outs() << "<<<<<<<<<<< Move the Rewriter from Visitor to Consumer >>>>>>>>>>>\n";
  }

};


//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class OperatorAnalysisAction : public clang::PluginASTAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef in_file){
      return std::unique_ptr<clang::ASTConsumer>(new OperatorAnalysisConsumer(&compiler));
    }

protected:
  bool ParseArgs(const clang::CompilerInstance &compiler, const std::vector<std::string> &args) override{
    return true;
  }
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static clang::FrontendPluginRegistry::Add<OperatorAnalysisAction>X("operator-analysis", "Predict which operators are likely to be in the target TACO program");
