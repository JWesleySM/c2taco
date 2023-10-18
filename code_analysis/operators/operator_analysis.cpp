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
#include "../code_analysis.h"

//-----------------------------------------------------------------------------
// RecursiveASTVisitor
//-----------------------------------------------------------------------------
class OperatorAnalysisVisitor : public clang::RecursiveASTVisitor<OperatorAnalysisVisitor>{
    
public:

  std::map<clang::VarDecl*, clang::BinaryOperator*>Definitions_;
  clang::Expr* ReturnValue_ = nullptr;
  bool ParseTACO_ = false;

  bool VisitTypedefDecl(clang::TypedefDecl *TD){
    if(TD->getNameAsString() == "taco_tensor_t")
      ParseTACO_ = true;
    
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *VD){
    if(!VD->isLocalVarDecl())
      return true;
    
    if(!VD->hasInit())
      return true;

    if(ParseTACO_)
      if(VD->getNameAsString().ends_with("_dimension") || VD->getNameAsString().ends_with("_vals"))
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

  std::string getOperatorAsString(clang::BinaryOperator* BO){
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


  void getOperatorsFromDefinition(clang::Stmt* Def, std::set<std::string>* Operators){
    if(clang::BinaryOperator* BO = clang::dyn_cast<clang::BinaryOperator>(Def))
      Operators->insert(getOperatorAsString(BO));
    
    // Do not add operators used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(Def))
      return;

    for(auto& Child : Def->children())
      getOperatorsFromDefinition(Child, Operators);
  }


  void getOperatorsFromLocalVariablesDefinitions(clang::Stmt* S, std::set<std::string>* Operators){
    if(clang::DeclRefExpr* Reference = clang::dyn_cast<clang::DeclRefExpr>(S)){
      clang::VarDecl* Decl = getVariableDeclaration(Reference);
      if(Decl){
        if(Decl->isLocalVarDecl()){
          clang::BinaryOperator* LocalVarDef = getVariableDefinition(Decl, Visitor_->Definitions_);
          if(LocalVarDef)
            getOperatorsFromDefinition(LocalVarDef->getRHS(), Operators);
        }
      }
    }

    // Do not add variables used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S))
      S = ASE->getBase();

    for(auto& Child : S->children())
      getOperatorsFromLocalVariablesDefinitions(Child, Operators);
      
  }


public:

  explicit OperatorAnalysisConsumer()
         :Visitor_(new OperatorAnalysisVisitor()){}


  std::set<std::string> getOperatorsInProgram(){
    std::set<std::string> Operators;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      // We found an assignment to the output variable
      if(isParameter(Var) || isReturn(Var, Visitor_->ReturnValue_) || mayBeOutputAlias(Var, Def)){
        getOperatorsFromDefinition(Def->getRHS(), &Operators);
        
        // Operators in the definition of local variables
        getOperatorsFromLocalVariablesDefinitions(Def->getRHS(), &Operators);

        // Operators resulting from compound assignments
        if(Def->isCompoundAssignmentOp()){
          if(Visitor_->ReturnValue_)
            continue;
          if(mayBeMM(Def, Visitor_->Definitions_))
            continue;

          Operators.insert(getOperatorAsString(Def));
        }
      }
    }
    return Operators;
  } 


  void HandleTranslationUnit(clang::ASTContext &Context) override{
    Visitor_->TraverseDecl(Context.getTranslationUnitDecl());
    for(std::string Op : getOperatorsInProgram())
      llvm::outs() << Op <<" ";
    llvm::outs() <<"\n";
  }

};


//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class OperatorAnalysisAction : public clang::PluginASTAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef in_file){
      return std::unique_ptr<clang::ASTConsumer>(new OperatorAnalysisConsumer());
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
