#ifndef _REXPR_H_
#define _REXPR_H_

#include "llvm/IR/IRBuilder.h"
//#include "llvm/IR/Module.h"
//#include "llvm/Support/Debug.h"

#include "ginac/ginac.h"

//#include <map>
#include <sstream>
#include <string>

using namespace llvm;
using std::map;
using std::pair;
using std::string;
using std::vector;

class RExpr;

class RExprMap {
public:
  RExpr operator[](const RExpr& Key);

  size_t size() const;

  GiNaC::exmap& getMap();

private:
  GiNaC::exmap Map_;
};


class RExpr {
public:
  RExpr();
  RExpr(int Int);
  RExpr(APInt Int);
  RExpr(GiNaC::ex Ex);
  RExpr(Twine Name);
  RExpr(Value *V);

  typedef struct __iterator {
    GiNaC::const_iterator It;
    RExpr operator*() {
      return RExpr(*It);
    }

    // Prefix.
    __iterator& operator++() {
      ++It;
      return *this;
    }

    // Postfix.
    __iterator operator++(int) {
      struct __iterator Copy = { It };
      ++It;
      return Copy;
    }

    bool operator==(__iterator& Other) {
      return It == Other.It;
    }

    bool operator!=(__iterator& Other) {
      return It != Other.It;
    }
  } iterator;

  iterator begin() { return { Expr_.begin() }; }
  iterator end()   { return { Expr_.end()   }; }

  bool isValid()    const;
  bool isAdd()      const;
  bool isMul()      const;
  bool isSymbol()   const;
  bool isConstant() const;

  bool eq        (const RExpr& Other) const;
  bool ne        (const RExpr& Other) const;
  bool operator==(const RExpr& Other) const;
  bool operator!=(const RExpr& Other) const;

  RExpr operator+(const RExpr& Other) const;
  RExpr operator+(unsigned Other)    const;
  RExpr operator-(const RExpr& Other) const;
  RExpr operator-(unsigned Other)    const;
  RExpr operator*(const RExpr& Other) const;
  RExpr operator*(unsigned Other)    const;
  RExpr operator/(const RExpr& Other) const;
  RExpr operator/(unsigned Other)    const;

  string getStringRepr() const;
  Value *getValue(unsigned BitWidth, LLVMContext &C,
                  IRBuilder<> &IRB) const;

  bool match(RExpr Ex, RExprMap& Map);
  bool match(RExpr Ex);
  bool has(RExpr Ex);

  RExpr subs(RExpr This, RExpr That) {
    return Expr_.subs(This.getExpr() == That.getExpr());
  }

  static RExpr InvalidExpr();
  static RExpr WildExpr();

  friend raw_ostream& operator<<(raw_ostream& OS, const RExpr& EI);
  friend RExpr RExprMap::operator[](const RExpr& Ex);

protected:
  GiNaC::ex getExpr() const;

private:
  GiNaC::ex Expr_;
};

#endif

