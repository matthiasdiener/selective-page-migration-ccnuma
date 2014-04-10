/*
 * GinacSupport.cpp
 *
 *  Created on: 03/04/2014
 *      Author: Simon Moll
 */

#include "llvm/Support/raw_ostream.h"
#include "ginac/ginac.h"
#include <sstream>

namespace llvm {

raw_ostream& operator<<(raw_ostream& OS, const GiNaC::ex &E) {
  std::ostringstream Str;
  Str << E;
  OS << Str.str();
  return OS;
}

}
