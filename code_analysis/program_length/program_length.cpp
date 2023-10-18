//==============================================================================
// FILE:
//    program_length.cpp
//
// AUTHOR:
//    José Wesley De Souza Magalhães [jwesley.magalhaes@ed.ac.uk | jwdesmagalhaes@gmail.com]
//
// DESCRIPTION:
//    Perform a static analysis to get the length of the target TACO program. In this context
//    length is defined as the number of array/pointer argument references and constants.                                                                                 
//
// USAGE:
//   clang -cc1 -load $BUILD_DIR/libProgramLength.so '\'
//    -plugin program-length program.c
// OR:
//   clang -c -Xclang -load -Xclang $BUILD_DIR/libProgramLength.so  '\'
//    -Xclang -plugin -Xclang program-length program.c
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
class ProgramLengthVisitor : public clang::RecursiveASTVisitor<ProgramLengthVisitor>{
    
public:

  std::map<clang::VarDecl*, clang::BinaryOperator*>Definitions_;
  clang::Expr* ReturnValue_ = nullptr;
  int TACOProgramLength_;
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

  bool VisitFunctionDecl(clang::FunctionDecl* FD){
    if(ParseTACO_){
      TACOProgramLength_ = FD->getNumParams() - 1;
      return false;
    }

    return true;
  }

};


//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class ProgramLengthConsumer : public clang::ASTConsumer{
private:
  ProgramLengthVisitor* Visitor_;

  void countTensorReferencesAndConstants(clang::Stmt* S, int* ProgramLength){
    // Variable Reference
    if(clang::DeclRefExpr* Reference = clang::dyn_cast<clang::DeclRefExpr>(S)){
      clang::VarDecl* Decl = getVariableDeclaration(Reference);
      if(Decl){
        // References to function parameters
        if(isParameter(Decl)){
          (*ProgramLength)++;
        }
        else if(Decl->isLocalVarDecl()){
          // Sum up references to parameters or constants in 
          // local variable definitions
          clang::BinaryOperator* LocalVarDef = getVariableDefinition(Decl, Visitor_->Definitions_);
          if(LocalVarDef)
            countTensorReferencesAndConstants(LocalVarDef->getRHS(), ProgramLength);
        }
      }        
    }

    // References to constants
    if(clang::isa<clang::IntegerLiteral>(S))
      (*ProgramLength)++;


    // Do not add variables used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S))
      S = ASE->getBase();

    for(auto& Child : S->children())
      countTensorReferencesAndConstants(Child, ProgramLength);
  }

public:

  explicit ProgramLengthConsumer()
         :Visitor_(new ProgramLengthVisitor()){}


  int getProgramLength(){
    int ProgramLength = 0;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      // We found an assignment to the output variable
      if(isParameter(Var) || isReturn(Var, Visitor_->ReturnValue_) || mayBeOutputAlias(Var, Def)){
        countTensorReferencesAndConstants(Def->getRHS(), &ProgramLength);

        // Increment 1 in case of compound assignments
        if(Def->isCompoundAssignmentOp()){
          if(Visitor_->ReturnValue_)
            continue;
          if(mayBeMM(Def, Visitor_->Definitions_))
            continue;

          ProgramLength++;
        }
      }
    }
    return ProgramLength;
  } 


  void HandleTranslationUnit(clang::ASTContext &Context) override{
    Visitor_->TraverseDecl(Context.getTranslationUnitDecl());
    int ProgramLength = Visitor_->ParseTACO_ ? Visitor_->TACOProgramLength_ : getProgramLength();
    llvm::outs() << ProgramLength <<"\n";
  }

};


//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class ProgramLengthAction : public clang::PluginASTAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef in_file){
      return std::unique_ptr<clang::ASTConsumer>(new ProgramLengthConsumer());
    }

protected:
  bool ParseArgs(const clang::CompilerInstance &compiler, const std::vector<std::string> &args) override{
    return true;
  }
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static clang::FrontendPluginRegistry::Add<ProgramLengthAction>X("program-length", "Get the expected length of the target TACO program");