//==============================================================================
// FILE:
//    tensor_orders.cpp
//
// AUTHOR:
//    José Wesley De Souza Magalhães [jwesley.magalhaes@ed.ac.uk | jwdesmagalhaes@gmail.com]
//
// DESCRIPTION:
//    Perform a static analysis to get the order of each tensor present in the TACO
//    version of the input program.                                                                                 
//
// USAGE:
//   clang -cc1 -load $BUILD_DIR/libTensorOrders.so '\'
//    -plugin tensor-orders program.c
// OR:
//   clang -c -Xclang -load -Xclang $BUILD_DIR/libTensorOrders.so  '\'
//    -Xclang -plugin -Xclang tensor-orders program.c
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
class TensorOrdersVisitor : public clang::RecursiveASTVisitor<TensorOrdersVisitor>{
    
public:
  
  explicit TensorOrdersVisitor(clang::ASTContext* context)
           :Rewriter_(clang::Rewriter(context->getSourceManager(), context->getLangOpts())){}

  clang::Rewriter Rewriter_;
  std::map<clang::VarDecl*, clang::BinaryOperator*>Definitions_;
  std::set<clang::VarDecl*> LoopIterators_;
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


  bool VisitForStmt(clang::ForStmt* FS){
    LoopIterators_.insert(getVariableDeclaration(getVariableReference(FS->getCond())));
    return true;
  }

};


//-----------------------------------------------------------------------------
// ASTConsumer
//-----------------------------------------------------------------------------
class TensorOrdersConsumer : public clang::ASTConsumer{
private:
  TensorOrdersVisitor* Visitor_;


  void getCoefficients(clang::Stmt* S, std::map<clang::VarDecl*, std::string>* Coeffs){
    if(clang::DeclRefExpr* Reference = clang::dyn_cast<clang::DeclRefExpr>(S)){
        Coeffs->insert({getVariableDeclaration(Reference), std::string("1")});
        return;
    }
    if(clang::BinaryOperator* BO = clang::dyn_cast<clang::BinaryOperator>(S)){
      if(BO->isMultiplicativeOp()){
        clang::VarDecl* LHS = getVariableDeclaration(getVariableReference(BO->getLHS()));
        clang::VarDecl* RHS = getVariableDeclaration(getVariableReference(BO->getRHS()));
        if(Visitor_->LoopIterators_.find(LHS) != Visitor_->LoopIterators_.end())
          Coeffs->insert({LHS, RHS->getNameAsString()});
        else
          Coeffs->insert({RHS, LHS->getNameAsString()});

        return;
      }
    }

    for(auto& Child : S->children())
      getCoefficients(Child, Coeffs);
  }


  int delinearize(clang::BinaryOperator* IndexExpr){ 
    std::map<clang::VarDecl*, std::string> U;
    getCoefficients(IndexExpr, &U);
    // If some iterator of surrounding loop is not present in index expression
    // its coefficient should be 0
    for(const auto& Li : Visitor_->LoopIterators_){
      if(U.find(Li) == U.end())
        U.insert({Li, std::string("0")});
    }

    int Order = 0;
    for(const auto& [Var, Coeff] : U){
      //llvm::outs() << Var->getNameAsString() << " -> " << Coeff << "\n";
      if(Coeff != "0")
        Order++;
    }
    
    return Order;
  }


  int getOrder(clang::Expr* E){
    // Constants and scalar references
    if(clang::isa<clang::DeclRefExpr>(E) || clang::isa<clang::IntegerLiteral>(E)){
      return 0;
    }else if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(E)){
      if(clang::ImplicitCastExpr* ICE = clang::dyn_cast<clang::ImplicitCastExpr>(ASE->getBase())){
        //2D Array delinearized indexation
        if(clang::isa<clang::ArraySubscriptExpr>(ICE->getSubExpr()))
          return 2;
      }
      // Array indexations with a unique variable reference
      if(clang::isa<clang::ImplicitCastExpr>(ASE->getIdx()) || clang::isa<clang::DeclRefExpr>(ASE->getIdx()))
        return 1;
      
      //Linearized indexation expressions
      if(clang::BinaryOperator* IndexExpr = clang::dyn_cast<clang::BinaryOperator>(ASE->getIdx()))
        return delinearize(IndexExpr);
    }
    return 200;
  }


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
    return false;
  }


  void getVariablesAndConstantsInDefinition(clang::Stmt* S, std::vector<clang::Expr*>* VarsAndCons){
    if(clang::DeclRefExpr* Reference = clang::dyn_cast<clang::DeclRefExpr>(S)){
        VarsAndCons->push_back(Reference);
    }

    if(clang::IntegerLiteral* Constant = clang::dyn_cast<clang::IntegerLiteral>(S))
      VarsAndCons->push_back(Constant);
    
    // Do not add variables used in array indexation expressions
    if(clang::ArraySubscriptExpr* ASE = clang::dyn_cast<clang::ArraySubscriptExpr>(S)){
      VarsAndCons->push_back(ASE);
      return;
    }

    for(auto& Child : S->children())
      getVariablesAndConstantsInDefinition(Child, VarsAndCons);
  }


  void getParemetersAndConstantsInLocalVarDefinition(clang::VarDecl* Decl, std::vector<clang::Expr*>* ParamsAndCons){
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      clang::VarDecl* AssignedVar = getVariableDeclaration(getVariableReference(Def->getLHS()));
      if(AssignedVar == Decl){
        std::vector<clang::Expr*> VarsAndCons;
        getVariablesAndConstantsInDefinition(Def->getRHS(), &VarsAndCons);
        for(auto& Exp : VarsAndCons){
          if(clang::isa<clang::IntegerLiteral>(Exp)){
            ParamsAndCons->push_back(Exp);
          }
          else{
            clang::VarDecl* V = getVariableDeclaration(getVariableReference(Exp));
            if(isParameter(V))
              ParamsAndCons->push_back(Exp);
          }
        }
      }
    }
  }

public:

  explicit TensorOrdersConsumer(clang::CompilerInstance *CI)
         :Visitor_(new TensorOrdersVisitor(&CI->getASTContext())){}

  std::vector<int> AnalyseDefinitions(){
    std::vector<int> Orders;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      //Def->dump();
      //llvm::outs() << "Definition: " << Visitor_->Rewriter_.getRewrittenText(Def->getSourceRange()) << "\n";
      clang::DeclRefExpr* Definition = getVariableReference(Def->getLHS());
      if(!Definition){
        llvm::errs() << "Could not get definition refered to in the assignment: " << Visitor_->Rewriter_.getRewrittenText(Def->getSourceRange()) << "\n";
        return Orders;
      }
      // We found an assignment to the output variable
      if(isParameter(Var) || isReturn(Var) || mayBeOutputAlias(Var, Def)){
        //llvm::outs() << "We need to analyze this\n";
        Orders.push_back(getOrder(Def->getLHS()));
        std::vector<clang::Expr*> VarsAndCons;
        getVariablesAndConstantsInDefinition(Def->getRHS(), &VarsAndCons);
        for(auto& Exp : VarsAndCons){
          //llvm::outs() << Visitor_->Rewriter_.getRewrittenText(Exp->getSourceRange()) << "\n";
          //Exp->dump();
          // Constants
          if(clang::isa<clang::IntegerLiteral>(Exp)){
            Orders.push_back(0);
          }
          else{
            clang::VarDecl* V = getVariableDeclaration(getVariableReference(Exp));
            // References to parameters
            if(isParameter(V)){
              Orders.push_back(getOrder(Exp));
            }
            // Local Variables need to have their own definition analysed
            else if(V->isLocalVarDecl()){
              std::vector<clang::Expr*> VarsAndConsInLocalVar;
              getParemetersAndConstantsInLocalVarDefinition(V, &VarsAndConsInLocalVar);
              for(auto& LocalVarExp : VarsAndConsInLocalVar){
                //llvm::outs() << Visitor_->Rewriter_.getRewrittenText(LocalVarExp->getSourceRange()) << "\n";
                Orders.push_back(getOrder(LocalVarExp));
              }
            }
          }
        }
        // In case of compound assignments, the order of the LHS must be duplicated
        if(Def->isCompoundAssignmentOp()){
          if(Visitor_->ReturnValue_)
            continue;
          if(mayBeMM(Def))
            continue;

          Orders.insert(Orders.begin() + 1, Orders[0]);
        //llvm::outs() << "We should not analyze this\n";
        }
      }
    }
    return Orders;
  }


  void HandleTranslationUnit(clang::ASTContext &Context) override{
    Visitor_->TraverseDecl(Context.getTranslationUnitDecl());
    for(int order : AnalyseDefinitions())
      llvm::outs() << order <<" ";
    llvm::outs() <<"\n";
    ////llvm::outs() << "<<<<<<<<<<< Move the Rewriter from Visitor to Consumer >>>>>>>>>>>\n";
  }

};


//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class TensorOrdersAction : public clang::PluginASTAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef in_file){
      return std::unique_ptr<clang::ASTConsumer>(new TensorOrdersConsumer(&compiler));
    }

protected:
  bool ParseArgs(const clang::CompilerInstance &compiler, const std::vector<std::string> &args) override{
    return true;
  }
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static clang::FrontendPluginRegistry::Add<TensorOrdersAction>X("tensor-orders", "Get order of each tensor in the corresponding TACO program");
