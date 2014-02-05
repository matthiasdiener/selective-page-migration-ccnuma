//===------------------------- PythonInterface.cpp- -----------------------===//
//===----------------------------------------------------------------------===//

#include "PythonInterface.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

/* ************************************************************************** */
/* ************************************************************************** */

using namespace llvm;

/* ************************************************************************** */
/* ************************************************************************** */

static cl::opt<bool>
  ClDebug("python-interface-debug",
          cl::desc("Enable debugging for the Python/LLVM interface"),
          cl::Hidden, cl::init(false));

static RegisterPass<PythonInterface>
  X("python-interface", "LLVM/Python interface");
char PythonInterface::ID = 0;

#define PI_DEBUG(X) { if (ClDebug) { X; } }

static cl::opt<bool>
  ClSPIDebug("sympy-interface-debug",
             cl::desc("Enable debugging for the SymPy/LLVM interface"),
             cl::Hidden, cl::init(false));

static RegisterPass<SymPyInterface>
  Y("sympy-interface", "SymPy/LLVM interface");
char SymPyInterface::ID = 0;

#define SPI_DEBUG(X) { if (ClSPIDebug) { X; } }

/* ************************************************************************** */
/* ************************************************************************** */

raw_ostream& operator<<(raw_ostream& OS, PyObject &Obj) {
  PyObject *Repr = PyObject_Repr(&Obj);
  OS << PyString_AsString(Repr);
  Py_DecRef(Repr);
  return OS;
}

/* ************************************************************************** */
/* ************************************************************************** */
bool PythonInterface::doInitialization(Module&) {
  static char *Argv[] = { "opt" };
  Py_Initialize();
  PySys_SetArgv(1, Argv);
  return true;
}

bool PythonInterface::doFinalization(Module&) {
  Py_Finalize();
  return true;
}

PyObject *PythonInterface::getModule(const char *Mod) {
  PyObject *ModStr = PyString_FromString(Mod);
  PyObject *ModObj = PyImport_Import(ModStr);
  Py_DecRef(ModStr);
  return ModObj;
}

PyObject *PythonInterface::getClass(const char *Mod, const char *Class) {
  PyEval_GetBuiltins();
  PyObject *DictObj = PyModule_GetDict(getModule(Mod));
  PyObject *ClassObj = PyDict_GetItemString(DictObj, Class);
  Py_DecRef(DictObj);
  return ClassObj;
}

PyObject *PythonInterface::getAttr(const char *Mod, const char *Class,
                                   const char *Attr) {
  PyObject *ClassObj = getClass(Mod, Class);
  PyObject *AttrObj = getAttr(ClassObj, Attr);
  Py_DecRef(ClassObj);
  return AttrObj;
}

PyObject *PythonInterface::getAttr(const char *Mod, const char *Attr) {
  return getAttr(getModule(Mod), Attr);
}

PyObject *PythonInterface::getAttr(PyObject *Obj, const char *Attr) {
  return PyObject_GetAttrString(Obj, Attr);
}

PyObject *PythonInterface::getBuiltin(const char *Attr) {
  PyObject *Builtins = PyEval_GetBuiltins();
  PyObject *AttrObj = PyDict_GetItemString(Builtins, Attr);
  Py_DecRef(Builtins);
  return AttrObj;
}

PyObject *PythonInterface::createTuple(std::initializer_list<PyObject*> Items) {
  PyObject *Tuple = PyTuple_New(Items.size());
  unsigned Idx = 0;
  for (auto &Item : Items)
    PyTuple_SetItem(Tuple, Idx++, Item);
  return Tuple;
}

PythonObjVec
    *PythonInterface::createObjVec(std::initializer_list<PythonObjInfo> Infos) {
  return createObjVec(std::vector<PythonObjInfo>(Infos));
}

PythonObjVec *PythonInterface::createObjVec(std::vector<PythonObjInfo> Infos) {
  std::vector<PyObject*> Functions;

  for (auto &Info : Infos) {
    PyObject *Fn;
    if (!strcmp(Info.Mod, "__builtins__")) {
      Fn = getBuiltin(Info.Fn);
    } else if (Info.Class) {
      if (Info.Fn)
        Fn = getAttr(Info.Mod, Info.Class, Info.Fn);
      else
        Fn = getClass(Info.Mod, Info.Class);
    } else {
      Fn = getAttr(Info.Mod, Info.Fn);
    }

    if (!Fn) {
      PI_DEBUG(
        if (Info.Class) {
          if (Info.Fn)
            dbgs() << "PythonInterface: could not get member function "
                   << Info.Mod << "." << Info.Class << "." << Info.Fn << "\n";
          else
            dbgs() << "PythonInterface: could not get class " << Info.Mod
                   << "." << Info.Class << "\n";
        } else {
          dbgs() << "PythonInterface: could not get module function "
                 << Info.Mod << "." << Info.Fn << "\n";
        }
      );
      return nullptr;
    }
    Functions.push_back(Fn);
  }

  return new PythonObjVec(Functions);
}

PyObject *PythonInterface::call(PyObject *Fn, PyObject *Tuple) {
  PI_DEBUG(dbgs() << "PythonInterface: call: " << *Fn << *Tuple << "\n");
  PyObject *Ret = PyObject_CallObject(Fn, Tuple);
  return Ret;
}

PyObject *PythonInterface::call(PythonObjVec *Vec, unsigned Idx,
                                PyObject *Tuple) {
  return call(Vec->getObj(Idx), Tuple);
}

PyObject *PythonInterface::call(PythonObjVec *Vec, unsigned Idx,
                                std::initializer_list<PyObject*> Items) {
  PyObject *Tuple = createTuple(Items);
  PyObject *Ret = call(Vec, Idx, Tuple);
  Py_DecRef(Tuple);
  return Ret;
}

PyObject *PythonInterface::call(PyObject *Fn,
                                std::initializer_list<PyObject*> Items) {
  PyObject *Tuple = createTuple(Items);
  PyObject *Ret = call(Fn, Tuple);
  Py_DecRef(Tuple);
  return Ret;
}

PyObject *PythonInterface::callSelf(PyObject *FnStr, PyObject *Self,
                                    PyObject *Tuple) {
  PyObject *FnObj = PyObject_GetAttr(Self, FnStr);
  PyObject *Ret = call(FnObj, Tuple);
  Py_DecRef(FnObj);
  return Ret;
}

PyObject *PythonInterface::callSelf(const char *Fn, PyObject *Self,
                                    PyObject *Tuple) {
  PyObject *FnStr = PyString_FromString(Fn);
  PyObject *Ret = callSelf(FnStr, Self, Tuple);
  Py_DecRef(FnStr);
  return Ret;
}

PyObject *PythonInterface::callSelf(const char *Fn, PyObject *Self,
                                    std::initializer_list<PyObject*> Items) {
  PyObject *Tuple = createTuple(Items);
  PyObject *Ret = callSelf(Fn, Self, Tuple);
  Py_DecRef(Tuple);
  return Ret;
}

bool SymPyInterface::runOnModule(Module&) {
  PI__ = &getAnalysis<PythonInterface>();

  ObjVec_ = PI__->createObjVec(getObjVecInit());
  assert(ObjVec_);

  return false;
}

PyObject *SymPyInterface::conv(Expr Ex) {
  if (Ex.isSymbol()) {
    PyObject *Var = var(Ex.getSymbolString().c_str());
    return Var;
  }
  else if (Ex.isInteger())
    return getInteger(Ex.getInteger());
  else if (Ex.isRational())
    return getRational(Ex.getRationalNumer(), Ex.getRationalDenom());
  else if (Ex.isFloat())
    return getFloat(Ex.getFloat());

  PyObject *Acc = nullptr;
  if (Ex.isAdd()) {
    for (auto SubEx : Ex)
      if (PyObject *Obj = conv(SubEx))
        Acc = Acc ? add(Acc, Obj) : Obj;
      else
        return nullptr;
  } else if (Ex.isMul()) {
    for (auto SubEx : Ex)
      if (PyObject *Obj = conv(SubEx))
        Acc = Acc ? mul(Acc, Obj) : Obj;
      else
        return nullptr;
  } else if (Ex.isPow()) {
    if (PyObject *Base = conv(Ex.at(0)))
      if (PyObject *Exp = conv(Ex.at(1)))
        Acc = pow(Base, Exp);
  } else if (Ex.isMin()) {
    if (PyObject *First = conv(Ex.at(0)))
      if (PyObject *Second = conv(Ex.at(1)))
        Acc = min(First, Second);
  } else if (Ex.isMax()) {
    if (PyObject *First = conv(Ex.at(0)))
      if (PyObject *Second = conv(Ex.at(1)))
        Acc = max(First, Second);
  } else {
    SPI_DEBUG(dbgs() << "SymPyInterface: error converting Expr " << Ex << "\n");
  }
  return Acc;
}

Expr SymPyInterface::conv(PyObject *Obj) {
  if (PyLong_Check(Obj)) {
    return Expr(PyLong_AsLong(Obj));
  } else if (PyType_IsSubtype(Obj->ob_type,
                              (PyTypeObject*)ObjVec_->getObj(CLS_SYMBOL))) {
    PyObject *Name = PI__->getAttr(Obj, "name");
    auto Ret = PyString_AsString(Name);
    Py_DecRef(Name);
    return Expr(std::string(Ret));
  } else if (PyType_IsSubtype(Obj->ob_type,
                              (PyTypeObject*)ObjVec_->getObj(CLS_INTEGER))) {
    PyObject *Val = PI__->callSelf("__int__", Obj, {});
    auto Ret = PyLong_AsLong(Val);
    Py_DecRef(Val);
    return Ret;
  } else if (PyType_IsSubtype(Obj->ob_type,
                              (PyTypeObject*)ObjVec_->getObj(CLS_RATIONAL))) {
    PyObject *Val = PI__->callSelf("as_numer_denom", Obj, {});
    auto Ret = Expr(PyLong_AsLong(PyTuple_GetItem(Val, 0)),
                    PyLong_AsLong(PyTuple_GetItem(Val, 1)));
    Py_DecRef(Val);
    return Ret;
  } else if (PyType_IsSubtype(Obj->ob_type,
                              (PyTypeObject*)ObjVec_->getObj(CLS_FLOAT))) {
    PyObject *Val = PI__->callSelf("__float__", Obj, {});
    auto Ret = PyFloat_AsDouble(Val);
    Py_DecRef(Val);
    return Ret;
  }

  PyObject *Args = PI__->getAttr(Obj, "args");
  assert(Args);

  std::vector<Expr> Exprs;
  for (unsigned Idx = 0; Idx < PyTuple_Size(Args); ++Idx)
    Exprs.push_back(conv(PyTuple_GetItem(Args, Idx)));

  Expr Ret;
  if (PyType_IsSubtype(Obj->ob_type, (PyTypeObject*)ObjVec_->getObj(CLS_ADD))) {
    for (auto &Ex : Exprs)
      Ret = Ret + Ex;
  } else if (PyType_IsSubtype(Obj->ob_type,
                              (PyTypeObject*)ObjVec_->getObj(CLS_MUL))) {
    Ret = (long)1;
    for (auto &Ex : Exprs)
      Ret = Ret * Ex;
  } else if (PyType_IsSubtype(Obj->ob_type,
                              (PyTypeObject*)ObjVec_->getObj(CLS_POW))) {
    Ret = Exprs.at(0) ^ Exprs.at(1);
  } else {
    SPI_DEBUG(dbgs() << "SymPyInterface: error converting PyObject* "
                     << *Obj << "\n");
  }
  return Ret;
}

PyObject *SymPyInterface::var(unsigned Val) {
  return PyInt_FromLong(Val);
}

PyObject *SymPyInterface::var(const char *Str) {
  return PI__->call(ObjVec_, CLS_SYMBOL, {PyString_FromString(Str)});
}

PyObject *SymPyInterface::getInteger(long Val) {
  return PyInt_FromLong(Val);
}

PyObject *SymPyInterface::getFloat(double Val) {
  return PyFloat_FromDouble(Val);
}

PyObject *SymPyInterface::getRational(long Numer, long Denom) {
  return PI__->call(ObjVec_, CLS_RATIONAL, {PyLong_FromLong(Numer),
                            PyLong_FromLong(Denom)});
}

PyObject *SymPyInterface::callBinOp(PyObject *Fn, PyObject *LHS,
                                    PyObject *RHS) {
  if (PyType_IsSubtype(LHS->ob_type, (PyTypeObject*)ObjVec_->getObj(CLS_EXPR)))
    return PI__->callSelf(Fn, LHS, {RHS});
  return PI__->callSelf(Fn, RHS, {LHS});
}

PyObject *SymPyInterface::callBinOp(const char *Fn, PyObject *LHS,
                                    PyObject *RHS) {
  if (PyType_IsSubtype(LHS->ob_type, (PyTypeObject*)ObjVec_->getObj(CLS_EXPR)))
    return PI__->callSelf(Fn, LHS, {RHS});
  return PI__->callSelf(Fn, RHS, {LHS});
}

PyObject *SymPyInterface::add(PyObject *LHS, PyObject *RHS) {
  return callBinOp("__add__", LHS, RHS);
}

PyObject *SymPyInterface::sub(PyObject *LHS, PyObject *RHS) {
  return callBinOp("__sub__", LHS, RHS);
}

PyObject *SymPyInterface::mul(PyObject *LHS, PyObject *RHS) {
  return callBinOp("__mul__", LHS, RHS);
}

PyObject *SymPyInterface::div(PyObject *LHS, PyObject *RHS) {
  return callBinOp("__div__", LHS, RHS);
}

PyObject *SymPyInterface::pow(PyObject *Base, PyObject *Exp) {
  return callBinOp("__pow__", Base, Exp);
}

PyObject *SymPyInterface::min(PyObject *First, PyObject *Second) {
  return PI__->call(ObjVec_, FN_MIN, {First, Second});
}

PyObject *SymPyInterface::max(PyObject *First, PyObject *Second) {
  return PI__->call(ObjVec_, FN_MAX, {First, Second});
}

PyObject *SymPyInterface::inverse(PyObject *Obj) {
  return PI__->call(ObjVec_, CLS_RATIONAL, {PyLong_FromLong(1), Obj});
}

PyObject *SymPyInterface::expand(PyObject *Obj) {
  return PI__->callSelf("expand", Obj, {});
}

PyObject *SymPyInterface::summation(PyObject *Expr, PyObject *Var,
                                    PyObject *Lower, PyObject *Upper) {
  PyObject *Tuple = PI__->createTuple({Var, Lower, Upper});
  PyObject *Ret = PI__->call(ObjVec_, FN_SUMMATION, {Expr, Tuple});
  Py_DecRef(Tuple);
  return Ret;
}

