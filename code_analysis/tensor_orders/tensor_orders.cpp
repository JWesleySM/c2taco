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
#include "../code_analysis.h"

//-----------------------------------------------------------------------------
// RecursiveASTVisitor
//-----------------------------------------------------------------------------
class TensorOrdersVisitor : public clang::RecursiveASTVisitor<TensorOrdersVisitor>{
    
public:

  std::map<clang::VarDecl*, clang::BinaryOperator*>Definitions_;
  std::set<clang::VarDecl*> LoopIterators_;
  std::set<clang::Stmt*> Loops_;
  clang::Expr* ReturnValue_ = nullptr;
  bool ParseTACO_ = false;
  std::vector<clang::VarDecl*>TACO_declarations_;


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

    // Save declarations of local variables in TACO-generated programs in a different data structure
    if(ParseTACO_)
      if(VD->getNameAsString().ends_with("_dimension") || VD->getNameAsString().ends_with("_vals")){
        TACO_declarations_.push_back(VD);     
        return true;
      }
      else{
        return true;
      }

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
    Loops_.insert(FS);
    return true;
  }


  bool VisitWhileStmt(clang::WhileStmt* WS){
    LoopIterators_.insert(getVariableDeclaration(getVariableReference(WS->getCond())));
    Loops_.insert(WS);
    return true;
  }


  bool VisitForStmt(clang::DoStmt* DS){
    LoopIterators_.insert(getVariableDeclaration(getVariableReference(DS->getCond())));
    Loops_.insert(DS);
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
    // its coefficient should be 0. In other words, it means the iterator
    // does not affect this tensor and it does count towards its order
    int TensorOrder = 0;
    for(const auto& Li : Visitor_->LoopIterators_){
      if(U.find(Li) != U.end())
        TensorOrder++;
    }
    
    return TensorOrder;
  }


  bool redefinesPointer(clang::Stmt* S, clang::VarDecl* Pointer){
    // If we find a definition of the pointer variable in the body of this loop
    // without moving it we do not consider a possible pointer movement 
    // because such definition means ponting to a specific part of the manipulated
    // array, e.g., the beginning or a next column
    if(clang::BinaryOperator* BO = clang::dyn_cast<clang::BinaryOperator>(S)){
      if(BO->isAssignmentOp()){
        clang::VarDecl* AssignedVar = getVariableDeclaration(getVariableReference(BO->getLHS()));
        if(AssignedVar == Pointer){
          if(!movesPointer(BO, Pointer))
            return true;
        }
      }
    }

    if(clang::ForStmt* FS = clang::dyn_cast<clang::ForStmt>(S))
      return false;

    for(auto& Child : S->children()){
      if(redefinesPointer(Child, Pointer))
        return true;
    }

    return false;    
  }


  bool movesPointer(clang::Stmt* S, clang::VarDecl* Pointer){
    if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(S)){
      if(UO->isIncrementOp() || UO->isDecrementOp()){    
        clang::VarDecl* Var = getVariableDeclaration(getVariableReference(S));
        if(Var == Pointer)
          return true;
      }
    }
    
    if(clang::ForStmt* FS = clang::dyn_cast<clang::ForStmt>(S))
      S = FS->getBody();

    for(auto& Child : S->children()){
      if(movesPointer(Child, Pointer))
        return true;
    }

    return false;
  }


  int recoverArray(clang::UnaryOperator* OP, bool IsOutput){
    clang::VarDecl* PointerVar = getVariableDeclaration(getVariableReference(OP));
    std::map<clang::VarDecl*, bool>Coeffs;
    for(auto& L : Visitor_->Loops_){
      clang::Expr* LoopCond = nullptr;
      clang::Stmt* LoopBody = nullptr;
      if(clang::ForStmt* FS = clang::dyn_cast<clang::ForStmt>(L)){
        LoopCond = FS->getCond();
        LoopBody = FS->getBody();
      }

      if(clang::WhileStmt* WS = clang::dyn_cast<clang::WhileStmt>(L)){
        LoopCond = WS->getCond();
        LoopBody = WS->getBody();
      }

      if(clang::DoStmt* DS = clang::dyn_cast<clang::DoStmt>(L)){
        LoopCond = DS->getCond();
        LoopBody = DS->getBody();
      }

      clang::VarDecl* LoopIterator = getVariableDeclaration(getVariableReference(LoopCond));
      Coeffs[LoopIterator] = movesPointer(LoopBody, PointerVar) && !(redefinesPointer(LoopBody, PointerVar) && !IsOutput);
    }

    int Order = 0;
    for(const auto& [Var, HasCoeff] : Coeffs){
      if(HasCoeff)
        Order++;
    }

    return Order;
  }


  int getOrder(clang::Expr* E, bool Output){
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
      
      // Linearized indexation expressions
      if(clang::BinaryOperator* IndexExpr = clang::dyn_cast<clang::BinaryOperator>(ASE->getIdx()))
        return delinearize(IndexExpr);
    }
    else if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(E)){
      // Array manipulation through pointers 
      if(UO->getOpcode() == clang::UnaryOperator::Opcode::UO_Deref)
        return recoverArray(UO, Output);

    }
    return 200;
  }


  void getOrderOfTensorsAndConstantsInDefinition(clang::Stmt* S, std::vector<int>*Orders){
    if(clang::isa<clang::DeclRefExpr>(S) || clang::isa<clang::ArraySubscriptExpr>(S)){
      clang::VarDecl* Decl = getVariableDeclaration(getVariableReference(S));
      if(Decl){
        // Reference to function parameters
        if(isParameter(Decl)){
          Orders->push_back(getOrder(clang::dyn_cast<clang::Expr>(S), false));
          return; 
        }
        else if(Decl->isLocalVarDecl()){
          // Get the order of references to parameters or constants
          // in local variable definitions
          clang::BinaryOperator* LocalVarDef = getVariableDefinition(Decl, Visitor_->Definitions_);
          if(LocalVarDef){
            getOrderOfTensorsAndConstantsInDefinition(LocalVarDef->getRHS(), Orders);
          }
        }
      }
    }

    // Reference to constants
    if(clang::isa<clang::IntegerLiteral>(S))
      Orders->push_back(0);
    
    if(clang::UnaryOperator* UO = clang::dyn_cast<clang::UnaryOperator>(S)){
      if(UO->getOpcode() == clang::UnaryOperator::Opcode::UO_Deref){
        // Array manipulation through pointers 
        Orders->push_back(getOrder(UO, false));
        return;
      }
    }

    for(auto& Child : S->children())
      getOrderOfTensorsAndConstantsInDefinition(Child, Orders);
  }

public:

  explicit TensorOrdersConsumer()
         :Visitor_(new TensorOrdersVisitor()){}

  std::vector<int>getOrdersInTACOProgram(){
    std::vector<int> Orders;
    for(auto& Var : Visitor_->TACO_declarations_){
      if(Var->getNameAsString().ends_with("_dimension")){
        if(Orders.empty())
          Orders.push_back(1);
        else
          Orders.back()++;
        }
      else if(Var->getNameAsString().ends_with("_vals")){
        Orders.push_back(0);
      }
    }

    Orders.pop_back();
    return Orders;
  }

  std::vector<int> AnalyseDefinitions(){
    if(Visitor_->ParseTACO_)
      return getOrdersInTACOProgram();
    
    std::vector<int> Orders;
    for(const auto& [Var, Def] : Visitor_->Definitions_){
      // We found an assignment to the output variable
      if(isParameter(Var) || isReturn(Var, Visitor_->ReturnValue_) || mayBeOutputAlias(Var, Def)){
        if(!Def->getSourceRange().isValid())
          continue;

        Orders.push_back(getOrder(Def->getLHS(), true));
        getOrderOfTensorsAndConstantsInDefinition(Def->getRHS(), &Orders);
          
        // In case of compound assignments, the order of the LHS must be duplicated
        if(Def->isCompoundAssignmentOp()){
          if(Visitor_->ReturnValue_)
            continue;
          if(mayBeMM(Def, Visitor_->Definitions_))
            continue;
  
          Orders.insert(Orders.begin() + 1, Orders[0]);
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
  }

};


//-----------------------------------------------------------------------------
// FrontendAction
//-----------------------------------------------------------------------------
class TensorOrdersAction : public clang::PluginASTAction{
public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &compiler, llvm::StringRef in_file){
      return std::unique_ptr<clang::ASTConsumer>(new TensorOrdersConsumer());
    }

protected:
  bool ParseArgs(const clang::CompilerInstance &compiler, const std::vector<std::string> &args) override{
    return true;
  }
};

//-----------------------------------------------------------------------------
// Registration
//-----------------------------------------------------------------------------
static clang::FrontendPluginRegistry::Add<TensorOrdersAction>X("tensor-orders", "Get order of each tensor in the target TACO program");
