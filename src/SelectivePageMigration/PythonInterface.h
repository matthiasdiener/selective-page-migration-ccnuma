#ifndef _PYTHONINTERFACE_H_
#define _PYTHONINTERFACE_H_

#include "Expr.h"

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include <python2.7/Python.h>
#include <vector>

using namespace llvm;

class PythonObjVec {
public:
  PythonObjVec(std::vector<PyObject*> Objs) : Objs_(Objs) { }

  PyObject *getObj(unsigned Idx) const {
    return Objs_.at(Idx);
  }

private:
  std::vector<PyObject*> Objs_;
};

struct PythonObjInfo {
  PythonObjInfo(const char *Mod, const char *Fn)
    : Mod(Mod), Class(nullptr), Fn(Fn) { }
  PythonObjInfo(const char *Mod, const char *Class, const char *Fn)
    : Mod(Mod), Class(Class), Fn(Fn) { }

  const char *Mod, *Class, *Fn;
};

class PythonInterface : public ModulePass {
public:
  static char ID;
  PythonInterface() : ModulePass(ID) { }

  virtual bool runOnModule(Module&) {
    return false;
  }

  virtual bool doInitialization(Module&);
  virtual bool doFinalization(Module&);

  PyObject *getModule (const char *Mod);
  PyObject *getClass  (const char *Mod, const char *Class);
  PyObject *getAttr   (const char *Mod, const char *Class, const char *Attr);
  PyObject *getAttr   (const char *Mod,                    const char *Attr);
  PyObject *getAttr   (PyObject *Obj,                      const char *Attr);
  PyObject *getBuiltin(const char *Attr);

  PyObject *createTuple(std::initializer_list<PyObject*> Items);

  PythonObjVec *createObjVec(std::initializer_list<PythonObjInfo> Infos);
  PythonObjVec *createObjVec(std::vector<PythonObjInfo> Infos);

  PyObject *call(PyObject *Fn, PyObject *Tuple);
  PyObject *call(PyObject *Fn, std::initializer_list<PyObject*> Items);
  PyObject *call(PythonObjVec *Vec, unsigned Idx, PyObject *Tuple);
  PyObject *call(PythonObjVec *Vec, unsigned Idx,
                 std::initializer_list<PyObject*> Items);

  PyObject *callSelf(const char *Fn, PyObject *Self,
                     std::initializer_list<PyObject*> Items);
  PyObject *callSelf(const char *Fn, PyObject *Self, PyObject *Tuple);
  PyObject *callSelf(PyObject *FnStr, PyObject *Self, PyObject *Tuple);
};

class SymPyInterface : public ModulePass {
public:
  static char ID;
  SymPyInterface() : ModulePass(ID) { }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<PythonInterface>();
    AU.setPreservesAll();
  }

  // Order seen in the vector below.
  enum {
    // sympy.functions.elementary.miscellaneous
    FN_MIN, FN_MAX,
    // sympy.concrete
    FN_SUMMATION,
    // sympy.core.expr.Expr
    CLS_EXPR, // FN_ADD, FN_SUB, FN_MUL, FN_DIV, FN_POW, FN_EVALF,
    // sympy.core.numbers.Number
    CLS_NUMBER, CLS_FLOAT, CLS_RATIONAL, CLS_INTEGER,
    // sympy.core.symbol.Symbol
    CLS_SYMBOL,
    // sympy.core.symbol.Add
    CLS_ADD,
    // sympy.core.symbol.Mul
    CLS_MUL,
    // sympy.core.symbol.Pow
    CLS_POW
  };

  static std::vector<PythonObjInfo> getObjVecInit() {
    return {
      // sympy.functions.elementary.miscellaneous
      PythonObjInfo("sympy.functions.elementary.miscellaneous",
                    "Min", nullptr),                         // FN_MIN
      PythonObjInfo("sympy.functions.elementary.miscellaneous",
                    "Max", nullptr),                         // FN_MAX
      // sympy.concrete
      PythonObjInfo("sympy.concrete", nullptr, "summation"), // FN_SUMMATION
      // sympy.core.expr
      PythonObjInfo("sympy.core.expr", "Expr", nullptr),     // CLS_EXPR
      // sympy.core.numbers
      PythonObjInfo("sympy.core.numbers", "Number",   nullptr), // CLS_NUMBER
      PythonObjInfo("sympy.core.numbers", "Float",    nullptr), // CLS_FLOAT
      PythonObjInfo("sympy.core.numbers", "Rational", nullptr), // CLS_RATIONAL
      PythonObjInfo("sympy.core.numbers", "Integer",  nullptr), // CLS_INTEGER
      // sympy.core.symbol
      PythonObjInfo("sympy.core.symbol", "Symbol", nullptr), // CLS_SYMBOL
      // sympy.core.add
      PythonObjInfo("sympy.core.add", "Add", nullptr),       // CLS_ADD
      // sympy.core.mul
      PythonObjInfo("sympy.core.mul", "Mul", nullptr),       // CLS_MUL
      // sympy.core.power
      PythonObjInfo("sympy.core.power", "Pow", nullptr)      // CLS_POW
    };
  }

  virtual bool runOnModule(Module&);

  PyObject *conv(Expr Ex);
  Expr      conv(PyObject *Obj);

  PyObject *var(unsigned Val);
  PyObject *var(const char *Str);
  PyObject *var(const Value *V);

  PyObject *getInteger(long Val);
  PyObject *getFloat(double Val);
  PyObject *getRational(long Numer, long Denom);

  PyObject *callBinOp(PyObject *FnObj, PyObject *LHS, PyObject *RHS);
  PyObject *callBinOp(const char *Fn, PyObject *LHS, PyObject *RHS);

  PyObject *add(PyObject *LHS, PyObject *RHS);
  PyObject *sub(PyObject *LHS, PyObject *RHS);
  PyObject *mul(PyObject *LHS, PyObject *RHS);
  PyObject *div(PyObject *LHS, PyObject *RHS);
  PyObject *pow(PyObject *Base, PyObject *Exp);

  PyObject *min(PyObject *First, PyObject *Second);
  PyObject *max(PyObject *First, PyObject *Second);

  PyObject *abs(PyObject *Obj);
  PyObject *inverse(PyObject *Obj);
  PyObject *expand(PyObject *Obj);

  PyObject *summation(PyObject *Expr, PyObject *Var,
                      PyObject *Lower, PyObject *Upper);

private:
  PythonInterface *PyInterface_;
  PythonObjVec *ObjVec_;
};

raw_ostream& operator<<(raw_ostream& OS, PyObject &Obj);

#endif

