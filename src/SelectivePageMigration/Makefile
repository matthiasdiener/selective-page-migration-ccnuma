##===- lib/Transforms/Reuse/Makefile -----------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

LEVEL = ../../..
LIBRARYNAME = SelectivePageMigration
LOADABLE_MODULE = 1
USEDLIBS =
REQUIRES_RTTI = 1

include $(LEVEL)/Makefile.common

CXX = g++
CXXFLAGS += -std=c++0x -Wno-deprecated-declarations -fexceptions -w
LIBS += -lginac -lpython2.7
LDFLAGS += -fPIC -shared -L/usr/local/lib -Wl,-rpath,/usr/local/lib:

