//===------------------------------ Expr.cpp ------------------------------===//
//===----------------------------------------------------------------------===//

#include "Expr.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

#include "ginac/ginac.h"
#include <sstream>
#include <unordered_map>

using namespace llvm;

/* ************************************************************************** */
/* ************************************************************************** */

static cl::opt<bool>
  ClDebug("expr-debug",
          cl::desc("Enable debugging for value-generating functions"),
          cl::Hidden, cl::init(false));

#define EXPR_DEBUG(X) { if (ClDebug) { X; } }

/* ************************************************************************** */
/* ************************************************************************** */

raw_ostream& operator<<(raw_ostream& OS, const GiNaC::ex &E) {
  std::ostringstream Str;
  Str << E;
  OS << Str.str();
  return OS;
}

raw_ostream& operator<<(raw_ostream& OS, const Expr &EI) {
  std::ostringstream Str;
  Str << EI.getExpr();
  OS << Str.str();
  return OS;
}

/* ************************************************************************** */
/* ************************************************************************** */

Expr ExprMap::operator[](const Expr& Ex) {
  return Expr(Map_[Ex.getExpr()]);
}

size_t ExprMap::size() const {
  return Map_.size();
}

GiNaC::exmap& ExprMap::getMap() {
  return Map_;
}

Expr::Expr() {
}

Expr::Expr(long Int)
  : Expr_(Int) {
}

Expr::Expr(long Numer, long Denom)
  : Expr_(GiNaC::numeric(Numer, Denom)) {
}

Expr::Expr(double Float)
  : Expr_(Float) {
}

Expr::Expr(APInt Int)
  : Expr_((long)Int.getSExtValue()) {
}

Expr::Expr(GiNaC::ex Ex)
  : Expr_(Ex) {
}

Expr::Expr(Twine Name)
  : Expr_(GiNaC::symbol(Name.str())) {
}

Expr::Expr(string Name)
  : Expr_(GiNaC::symbol(Name)) {
}

static std::unordered_map<Value*, GiNaC::ex>   Exprs;
static std::unordered_map<Value*, unsigned>    Ids;
static std::unordered_map<std::string, Value*> Values;

static std::string GetName(Value *V) {
  static unsigned Id = 0;
  if (!Ids.count(V))
    Ids[V] = Id++;

  Twine Name;
  if (V->hasName()) {
    if (isa<Instruction>(V) || isa<Argument>(V))
      Name = V->getName();
    else
      Name = "__SRA_SYM_UNKNOWN_" + V->getName() + "__";
  } else {
    Name = "__SRA_SYM_UNAMED__";
  }
  return Name.str() + "." + std::to_string(Ids[V]);
}

Expr::Expr(Value *V) {
  assert(V && "Constructor expected non-null parameter");

  if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
    Expr_ = GiNaC::ex((long int)CI->getValue().getSExtValue());
    return;
  }

  if (Exprs.count(V)) {
    Expr_ = Exprs[V];
    return;
  }

  string NameStr = GetName(V);
  Expr_ = GiNaC::symbol(NameStr);
  Exprs[V] = Expr_;
  Values[NameStr] = V;
}

Expr Expr::at(unsigned Idx) const {
  return Expr_.op(Idx);
}

size_t Expr::nops() const {
  return Expr_.nops();
}

bool Expr::isValid() const {
  return *this != InvalidExpr();
}

bool Expr::isSymbol() const {
  return GiNaC::is_a<GiNaC::symbol>(Expr_);
}

bool Expr::isAdd() const {
  return GiNaC::is_a<GiNaC::add>(Expr_);
}

bool Expr::isMul() const {
  return GiNaC::is_a<GiNaC::mul>(Expr_);
}

bool Expr::isPow() const {
  return GiNaC::is_a<GiNaC::power>(Expr_);
}

bool Expr::isMin() const {
  return GiNaC::is_a<GiNaC::function>(Expr_) &&
         GiNaC::ex_to<GiNaC::function>(Expr_).get_name() == "min";
}

bool Expr::isMax() const {
  return GiNaC::is_a<GiNaC::function>(Expr_) &&
         GiNaC::ex_to<GiNaC::function>(Expr_).get_name() == "max";
}

bool Expr::isConstant() const {
  return GiNaC::is_a<GiNaC::numeric>(Expr_);
}

bool Expr::isInteger() const {
  return GiNaC::is_a<GiNaC::numeric>(Expr_) &&
         GiNaC::ex_to<GiNaC::numeric>(Expr_).is_integer();
}

bool Expr::isRational() const {
  return GiNaC::is_a<GiNaC::numeric>(Expr_) &&
         GiNaC::ex_to<GiNaC::numeric>(Expr_).is_rational();
}

bool Expr::isFloat() const {
  return GiNaC::is_a<GiNaC::numeric>(Expr_) &&
         GiNaC::ex_to<GiNaC::numeric>(Expr_).is_real();
}

bool Expr::isPositive() const {
  assert(isConstant() && "Expected constant expression");
  return GiNaC::ex_to<GiNaC::numeric>(Expr_).is_positive();
}

bool Expr::isNegative() const {
  assert(isConstant() && "Expected constant expression");
  return GiNaC::ex_to<GiNaC::numeric>(Expr_).is_negative();
}

long Expr::getInteger() const {
  return GiNaC::ex_to<GiNaC::numeric>(Expr_).to_long();
}

double Expr::getFloat() const {
  return GiNaC::ex_to<GiNaC::numeric>(Expr_).to_double();
}

long Expr::getRationalNumer() const {
  return GiNaC::ex_to<GiNaC::numeric>(Expr_).numer().to_long();
}

long Expr::getRationalDenom() const {
  return GiNaC::ex_to<GiNaC::numeric>(Expr_).denom().to_long();
}

string Expr::getSymbolString() const {
  std::ostringstream Str;
  Str << Expr_;
  return Str.str();
}

vector<Expr> Expr::getSymbols() const {
  vector<Expr> Symbols;
  for (auto Ex = preorder_begin(), E = preorder_end(); Ex != E; ++Ex)
    if ((*Ex).isSymbol())
      Symbols.push_back(*Ex);
  return Symbols;
}

Expr Expr::getPowBase() const {
  assert(isPow() && "Expected power expression");
  return at(0);
}

Expr Expr::getPowExp() const {
  assert(isPow() && "Expected power expression");
  return at(1);
}

Value *Expr::getSymbolValue() const {
  return Values[GiNaC::ex_to<GiNaC::symbol>(Expr_).get_name()];
}

Value *Expr::getValue(IntegerType *Ty,
                      IRBuilder<> &IRB) const {
  // FIXME: Check if it's a rational. Divide and cast to float if so.
  if (isInteger()) {
    APInt Int(Ty->getBitWidth(), getInteger(), true);
    return Constant::getIntegerValue(Ty, Int);
  } else if (isRational() || isFloat()) {
    APInt Int(Ty->getBitWidth(), (long)getFloat(), true);
    return Constant::getIntegerValue(Ty, Int);
  } else if (isSymbol()) {
    Value *V = Values[getSymbolString()];
    if (V->getType() == Ty)
      return V;
    else if (V->getType()->getIntegerBitWidth() < Ty->getBitWidth())
      return IRB.CreateSExt(V, Ty);
    else
      return IRB.CreateTrunc(V, Ty);
  }
  return nullptr;
}

Value *Expr::getValue(unsigned BitWidth, LLVMContext &C,
                      IRBuilder<> &IRB) const {
  return getValue(IntegerType::get(C, BitWidth), IRB);
}

Value *Expr::getExprValue(unsigned BitWidth, IRBuilder<> &IRB,
                          Module *M) const {
  IntegerType *Ty = IntegerType::get(M->getContext(), BitWidth);
  return getExprValue(Ty, IRB, M);
}

Value *Expr::getExprValue(IntegerType *Ty, IRBuilder<> &IRB, Module *M) const {
  LLVMContext &C = M->getContext();
  // Stop recursion when the expression is a single atom - a symbol or a
  // constant.
  if (isSymbol() || isConstant()) {
    Value *V = getValue(Ty, IRB);
    EXPR_DEBUG(dbgs() << "SelectivePageMigration: value for " << Expr_
                      << " is: " << *V << "\n");
    return V;
  } else if (isPow()) {
    Value *Base = getPowBase().getExprValue(Ty, IRB, M);
    Value *Exp  = getPowExp().getExprValue(Ty, IRB, M);

    Type  *DoubleTy   = Type::getDoubleTy(C);
    Value *BaseDouble = IRB.CreateSIToFP(Base, DoubleTy);
    Value *ExpDouble  = IRB.CreateSIToFP(Exp,  DoubleTy);

    Function *PowFn = Intrinsic::getDeclaration(M, Intrinsic::pow,
                                                ArrayRef<Type*>(DoubleTy));
    Value *Pow = IRB.CreateCall2(PowFn, BaseDouble, ExpDouble);
    Value *Cast = IRB.CreateFPToSI(Pow, Base->getType());
    EXPR_DEBUG(dbgs() << "SelectivePageMigration: value for " << Expr_
                      << " is: " << *Cast << "\n");
    return Cast;
  } else if (isAdd() || isMul()) {
    bool IsAdd = isAdd(), IsMul = isMul();
    // The accumulator will keep track of the last generated expression, to be
    // used in the next iteration.
    Value *Acc = nullptr;
    for (auto SubEx : (Expr)*this) {
      // Rationals should generate an sdiv/udiv instruction in multiplications.
      if (IsMul) {
        if (!SubEx.isInteger() && SubEx.isRational()) {
          APInt Int;
          
          Int = APInt(Ty->getBitWidth(), SubEx.getRationalNumer(), true);
          Constant *Numer = Constant::getIntegerValue(Ty, Int);
          Acc = Acc ? IRB.CreateMul(Acc, Numer) : Numer;

          Int = APInt(Ty->getBitWidth(), SubEx.getRationalDenom(), true);
          Constant *Denom = Constant::getIntegerValue(Ty, Int);
          Acc = IRB.CreateSDiv(Acc, Denom);

          continue;
        }
      }

      Value *Curr = SubEx.getExprValue(Ty, IRB, M);
      Acc = Acc ? IsAdd ? IRB.CreateAdd(Acc, Curr) : IRB.CreateMul(Acc, Curr)
                : Curr;
    }
    EXPR_DEBUG(dbgs() << "SelectivePageMigration: value for " << Expr_
                      << " is: " << *Acc << "\n");
    return Acc;
  } else if (isMin()) {
    Value *Left  = at(0).getExprValue(Ty, IRB, M);
    Value *Right = at(1).getExprValue(Ty, IRB, M);
    Value *Cmp = IRB.CreateICmp(CmpInst::ICMP_SLT, Left, Right);
    Value *Ret = IRB.CreateSelect(Cmp, Left, Right);
    EXPR_DEBUG(dbgs() << "SelectivePageMigration: value for " << Expr_
                      << " is: " << *Ret << "\n");
    return Ret;
  } else if (isMax()) {
    Value *Left  = at(0).getExprValue(Ty, IRB, M);
    Value *Right = at(1).getExprValue(Ty, IRB, M);
    Value *Cmp = IRB.CreateICmp(CmpInst::ICMP_SGT, Left, Right);
    Value *Ret = IRB.CreateSelect(Cmp, Left, Right);
    EXPR_DEBUG(dbgs() << "SelectivePageMigration: value for " << Expr_
                      << " is: " << *Ret << "\n");
    return Ret;
  } else {
    EXPR_DEBUG(dbgs() << "SelectivePageMigration: unhandled expression: "
                      << Expr_ << "\n");
    return nullptr;
  }
}

bool Expr::eq(const Expr& Other) const {
  return Expr_.is_equal(Other.getExpr());
}

bool Expr::ne(const Expr& Other) const {
  return !Expr_.is_equal(Other.getExpr());
}

bool Expr::operator==(const Expr& Other) const {
  return eq(Other);
}

bool Expr::operator!=(const Expr& Other) const {
  return ne(Other);
}

Expr Expr::operator+(const Expr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_ + Other.getExpr();
  return InvalidExpr();
}

Expr Expr::operator+(unsigned Other) const {
  if (isValid())
    return Expr_ + Other;
  return InvalidExpr();
}

Expr Expr::operator-(const Expr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_ - Other.getExpr();
  return InvalidExpr();
}

Expr Expr::operator-(unsigned Other) const {
  if (isValid())
    return Expr_ - Other;
  return InvalidExpr();
}

Expr Expr::operator*(const Expr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_ * Other.getExpr();
  return InvalidExpr();
}

Expr Expr::operator*(unsigned Other) const {
  if (isValid())
    return Expr_ * Other;
  return InvalidExpr();
}

Expr Expr::operator/(const Expr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_/Other.getExpr();
  return InvalidExpr();
}

Expr Expr::operator/(unsigned Other) const {
  if (isValid())
    return Expr_/Other;
  return InvalidExpr();
}

Expr Expr::operator^(const Expr& Other) const {
  if (isValid() && Other.isValid())
    return pow(Expr_, Other.getExpr());
  return InvalidExpr();
}

Expr Expr::operator^(unsigned Other) const {
  if (isValid())
    return pow(Expr_, Other);
  return InvalidExpr();
}

Expr Expr::min(Expr Other) const {
  if (isValid() && Other.isValid())
    return GiNaC::min(Expr_, Other.getExpr()).eval();
  return InvalidExpr();
}

Expr Expr::max(Expr Other) const {
  if (isValid() && Other.isValid())
    return GiNaC::max(Expr_, Other.getExpr()).eval();
  return InvalidExpr();
}

Expr Expr::subs(Expr This, Expr That) const {
  return Expr_.subs(This.getExpr() == That.getExpr());
}

bool Expr::match(Expr Ex, ExprMap& Map) const {
  return Expr_.match(Ex.getExpr(), Map.getMap());
}

bool Expr::match(Expr Ex) const {
  return Expr_.match(Ex.getExpr());
}

bool Expr::has(Expr Ex) const {
  return Expr_.has(Ex.getExpr());
}

Expr Expr::InvalidExpr() {
  static Expr Invalid(string("__INVALID__"));
  return Invalid;
}

Expr Expr::WildExpr() {
  return GiNaC::wild();
}

GiNaC::ex Expr::getExpr() const {
  return Expr_;
}

