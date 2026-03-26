# 3.15.0a7 Wii Cross-Build Progress (2.0 Makefile Port)

## Steps from Plan:
- [x] 1. Create TODO.md with steps.
- [x] 2. Port 2.0 Makefile → 3.15.0a7/Makefile (cross-tools, no host/configure).
- [x] 3. Create stub Makefiles for Parser/Grammar/Objects/Python/Modules (add2lib).
- [x] 4. Test compile: cd 3.15.0a7 && make clean all (zlib OK, Parser fails pyconfig/Python.h).
- [x] 5. Fix pyconfig.h (add Py_ssize_t etc.).
- [x] 6. Retest compile (still  onfails SIZEOF_WCHAR_T, pycore_pystate.h, gen files; needs host Python + configure).
- [ ] 7. Test wiitest.
- [ ] 8. Complete.
