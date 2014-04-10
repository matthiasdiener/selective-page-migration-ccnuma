# Description
RangedAddressSanitizer applies optimizations for AddressSanitizer (included in LLVM/clang).
It detects affine array accesses in loop nests and hoists the sanity checks out into a ranged check.

# Build requirements
Same as for SelectivePageMigration (SymPy, GiNaC, etc)
LLVM 3.4.1 (RTTI build)
Clang 3.4.1 (apply the git patches in clang_patches/)

# Installation Instructions
1. Copy contents into lib/Transforms/Instrumentation/
2. Run make in Runtime/
3. Let FASANMODULE point to the BC runtime module

# Usage instructions
With the patch applied, ASan will always run with the optimization enabled (-fsanitize=address).

# Options
if FASAN_DISABLE is set to any value, AddressSanitizer without range optimizations applied.

# Remarks
Eventhough included, RangedAddressSanitizer does not make use of RelativeExecutions (the reuse constant is set to 0).