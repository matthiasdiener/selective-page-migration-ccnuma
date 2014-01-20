//===------------------------------ RExpr.cpp -----------------------------===//
//===----------------------------------------------------------------------===//

#include "RExpr.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "ginac/ginac.h"

#include <algorithm>
#include <unordered_map>
#include <queue>
#include <set>

/* ************************************************************************** */
/* ************************************************************************** */

using namespace llvm;
// using std::make_pair;
// using std::map;
// using std::max;
// using std::pair;
// using std::queue;
// using std::set;
// using std::string;
// using std::stringstream;

namespace llvm {

raw_ostream& operator<<(raw_ostream& OS, const GiNaC::ex &E);

}

/* ************************************************************************** */
/* ************************************************************************** */

/*********
 * RExpr *
 *********/
RExpr::RExpr() {
}

RExpr::RExpr(int Int)
  : Expr_(Int) {
}

RExpr::RExpr(APInt Int)
  : Expr_((long int)Int.getSExtValue()) {
}

RExpr::RExpr(GiNaC::ex Expr)
  : Expr_(Expr) {
}

RExpr::RExpr(Twine Name)
  : Expr_(GiNaC::symbol(Name.str())) {
}

static std::map<Value*, GiNaC::ex> Exprs;
static std::map<Value*, unsigned>  Ids;
static std::map<std::string, Value*> Values;

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

RExpr::RExpr(Value *V) {
  assert(V && "Constructor expected non-null parameter");

  if (ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
    Expr_ = GiNaC::ex((long int)CI->getValue().getSExtValue());
    return;
  }

  if (Exprs.count(V)) {
    Expr_ = Exprs[V];
    return;
  }

  std::string NameStr = GetName(V);
  Expr_ = GiNaC::ex(GiNaC::symbol(NameStr));
  Exprs[V] = Expr_;
  Values[NameStr] = V;
}

bool RExpr::isValid() const {
  return *this != InvalidExpr();
}

bool RExpr::isAdd() const {
  return GiNaC::is_a<GiNaC::add>(Expr_);
}

bool RExpr::isMul() const {
  return GiNaC::is_a<GiNaC::mul>(Expr_);
}

bool RExpr::isConstant() const {
  return GiNaC::is_a<GiNaC::numeric>(Expr_);
}

bool RExpr::isSymbol() const {
  return GiNaC::is_a<GiNaC::symbol>(Expr_);
}

bool RExpr::eq(const RExpr& Other) const {
  return Expr_.is_equal(Other.getExpr());
}

bool RExpr::ne(const RExpr& Other) const {
  return !Expr_.is_equal(Other.getExpr());
}

bool RExpr::operator==(const RExpr& Other) const {
  return eq(Other);
}

bool RExpr::operator!=(const RExpr& Other) const {
  return ne(Other);
}

RExpr RExpr::operator+(const RExpr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_ + Other.getExpr();
  return InvalidExpr();
}

RExpr RExpr::operator+(unsigned Other) const {
  if (isValid())
    return Expr_ + Other;
  return InvalidExpr();
}

RExpr RExpr::operator-(const RExpr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_ - Other.getExpr();
  return InvalidExpr();
}

RExpr RExpr::operator-(unsigned Other) const {
  if (isValid())
    return Expr_ - Other;
  return InvalidExpr();
}

RExpr RExpr::operator*(const RExpr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_ * Other.getExpr();
  return InvalidExpr();
}

RExpr RExpr::operator*(unsigned Other) const {
  if (isValid())
    return Expr_ * Other;
  return InvalidExpr();
}

RExpr RExpr::operator/(const RExpr& Other) const {
  if (isValid() && Other.isValid())
    return Expr_/Other.getExpr();
  return InvalidExpr();
}

RExpr RExpr::operator/(unsigned Other) const {
  if (isValid())
    return Expr_/Other;
  return InvalidExpr();
}

GiNaC::ex RExpr::getExpr() const {
  return Expr_;
}

string RExpr::getStringRepr() const {
  std::ostringstream Str;
  Str << Expr_;
  return Str.str();
}

Value *RExpr::getValue(unsigned BitWidth, LLVMContext &C,
                      IRBuilder<> &IRB) const {
  if (GiNaC::is_a<GiNaC::numeric>(Expr_)) {
    APInt Int(BitWidth, GiNaC::ex_to<GiNaC::numeric>(Expr_).to_long(), true);
    return Constant::getIntegerValue(IntegerType::get(C, BitWidth), Int);
  } else if (GiNaC::is_a<GiNaC::symbol>(Expr_)) {
    IntegerType *Ty = IntegerType::get(C, BitWidth);
    Value *V = Values[GiNaC::ex_to<GiNaC::symbol>(Expr_).get_name()];
    if (V->getType() == Ty)
      return V;
    else if (V->getType()->getIntegerBitWidth() < BitWidth)
      return IRB.CreateSExt(V, Ty);
    else
      return IRB.CreateTrunc(V, Ty);
  }
  return nullptr;
}

bool RExpr::match(RExpr Ex, RExprMap& Map) {
  return Expr_.match(Ex.getExpr(), Map.getMap());
}

bool RExpr::match(RExpr Ex) {
  return Expr_.match(Ex.getExpr());
}

bool RExpr::has(RExpr Ex) {
  return Expr_.has(Ex.getExpr());
}

RExpr RExpr::InvalidExpr() {
  static RExpr Invalid(GiNaC::symbol("__INVALID__"));
  return Invalid;
}

RExpr RExpr::WildExpr() {
  return GiNaC::wild();
}

namespace llvm {

raw_ostream& operator<<(raw_ostream& OS, const GiNaC::ex &E) {
  std::ostringstream Str;
  Str << E;
  OS << Str.str();
  return OS;
}

} // end namespace llvm

raw_ostream& operator<<(raw_ostream& OS, const RExpr &EI) {
  std::ostringstream Str;
  Str << EI.getExpr();
  OS << Str.str();
  return OS;
}

/************
 * RExprMap *
 ************/
RExpr RExprMap::operator[](const RExpr& Ex) {
  return RExpr(Map_[Ex.getExpr()]);
}

size_t RExprMap::size() const {
  return Map_.size();
}

GiNaC::exmap& RExprMap::getMap() {
  return Map_;
}

