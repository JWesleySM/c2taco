//==============================================================================
// FILE:
//    ProgramLength.cpp
//
// AUTHOR:
//    José Wesley De Souza Magalhães [jwesley.magalhaes@ed.ac.uk | jwdesmagalhaes@gmail.com]
//
// DESCRIPTION:
//    Perform a static analysis to get the length of the program. In this context length is defined
//    as the number of array/pointer argument references and constants.                                                                                 
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
class ProgramLengthVisitor : public clang::RecursiveASTVisitor<ProgramLengthVisitor>{
    
public:
  
  explicit ProgramLengthVisitor(clang::ASTContext* context)
           :Rewriter_(clang::Rewriter(context->getSourceManager(), context->getLangOpts())){}

  clang::Rewriter Rewriter_;
  //std::vector<clang::BinaryOperator*>Definitions_;
  std::map<clang::VarDecl*, clang::BinaryOperator*>Definitions_;
  clang::Expr* ReturnValue_ = nullptr;


  bool VisitVarDecl(clang::VarDecl *VD){
    if(!VD->isLocalVarDecl())
      return true;
    
    if(!VD->hasInit())
      return true;

    
    ////llvm::outs() << "Initialization of variable " << Rewriter_.getRewrittenText(VD->getSourceRange()) << " :: " << Rewriter_.getRewrittenText(VD->getInit()->getSourceRange()) << "\n";
    clang::DeclRefExpr* VarReference = clang::DeclRefExpr::CreateEmpty(VD->getASTContext(), false, true, false, 0);
    VarReference->setType(VD->getType());
    VarReference->setDecl(VD);
    clang::BinaryOperator* VarInit = clang::BinaryOperator::CreateEmpty(VD->getASTContext(), false);
    VarInit->setLHS(VarReference);
    VarInit->setRHS(VD->getInit());
    VarInit->setOpcode(clang::BinaryOperator::Opcode::BO_Assign);
    //Definitions_.push_back(VarInit);
    Definitions_[VD] = VarInit;
    return true;  
  }

  bool VisitBinaryOperator(clang::BinaryOperator* BO){
    if(!BO->isAssignmentOp())
      return true;
    
    //Definitions_.push_back(BO);
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
class ProgramLengthConsumer : public clang::ASTConsumer{
private:
  ProgramLengthVisitor* Visitor_;

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
    // that corresponds to a matrix multiplication kernel, so we do not add the  order of the 
    // output array again on the orders vector even though this is an compound assignment
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


  void getConstantsInDefinition(clang::Stmt* S, std::vector<clang::IntegerLiteral*>* Cons){
    if(clang::IntegerLiteral* Constant = clang::dyn_cast<clang::IntegerLiteral>(S))
      Cons->push_back(Constant);

    // Do not add variables used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S)){
      S = ASE->getBase();
    }

    for(auto& Child : S->children())
      getConstantsInDefinition(Child, Cons);
  } 


  int getNumOfParemetersInVarDefinition(clang::VarDecl* Decl){
    int NumOfParametersInVarDefinition = 0;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      clang::VarDecl* AssignedVar = getVariableDeclaration(getVariableReference(Def->getLHS()));
      if(AssignedVar == Decl){
        std::vector<clang::VarDecl*> Vars;
        getVariablesInDefinition(Def->getRHS(), &Vars);
        for(auto& V : Vars){
          ////llvm::outs() << "Variables used in definition of local: " << Visitor_->Rewriter_.getRewrittenText(V->getSourceRange()) << "\n";
          if(isParameter(V))
           NumOfParametersInVarDefinition++;
        }
      }

    }
    return NumOfParametersInVarDefinition;
  }


public:

  explicit ProgramLengthConsumer(clang::CompilerInstance *CI)
         :Visitor_(new ProgramLengthVisitor(&CI->getASTContext())){}


  int AnalyseDefinitions(){
    int ProgramLength = 0;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      //Def->dump();
      //llvm::outs() << "Definition: " << Visitor_->Rewriter_.getRewrittenText(Def->getSourceRange()) << "\n";
      clang::DeclRefExpr* Definition = getVariableReference(Def->getLHS());
      if(!Definition){
        llvm::errs() << "Could not get definition refered to in the assignment: " << Visitor_->Rewriter_.getRewrittenText(Def->getSourceRange()) << "\n";
        return 0;
      }
      // We found an assignment to the output variable
      if(isParameter(Var) || isReturn(Var) || mayBeOutputAlias(Var, Def)){
        //llvm::outs() << "We need to analyze this\n";
        std::vector<clang::VarDecl*> Vars;
        getVariablesInDefinition(Def->getRHS(), &Vars);

        // References to parameters
        for(auto& V : Vars){
          if(isParameter(V))
            ProgramLength++;
          else if(V->isLocalVarDecl()){
            ProgramLength += getNumOfParemetersInVarDefinition(V);
          }
        }

        // Constants
        std::vector<clang::IntegerLiteral*> Cons;
        getConstantsInDefinition(Def->getRHS(), &Cons);
        ProgramLength += Cons.size();

        // Increment 1 in case of compound assignments
        if(Def->isCompoundAssignmentOp()){
          if(Visitor_->ReturnValue_)
            continue;
          if(mayBeMM(Def))
            continue;

          ProgramLength++;
        }
        
      }
      else{
        //llvm::outs() << "We should not analyze this\n";
      }
    }
    ////llvm::outs() << "Length of TACO program: " << ProgramLength << "\n";
    return ProgramLength;
  } 


  void HandleTranslationUnit(clang::ASTContext &Context) override{
    Visitor_->TraverseDecl(Context.getTranslationUnitDecl());
    llvm::outs() << AnalyseDefinitions() <<"\n";
    ////llvm::outs() << "<<<<<<<<<<< Move the Rewriter from Visitor to Consumer >>>>>>>>>>>\n";
  }

};


//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class ProgramLengthAction : public clang::PluginASTAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef in_file){
      return std::unique_ptr<clang::ASTConsumer>(new ProgramLengthConsumer(&compiler));
    }

protected:
  bool ParseArgs(const clang::CompilerInstance &compiler, const std::vector<std::string> &args) override{
    return true;
  }
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static clang::FrontendPluginRegistry::Add<ProgramLengthAction>X("program-length", "Get the expected length of the corresponding TACO program");
