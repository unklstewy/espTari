# 4.3 Loader lifecycle

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 08

## 4.3 Loader lifecycle

1. Read machine profile request
2. Resolve module set from catalog
3. Validate ABI compatibility and dependencies
4. Validate integrity/signature
5. Allocate runtime region(s)
6. Load + relocate EBIN(s)
7. Bind exported interfaces to engine contracts
8. Run module init hooks
9. Transition engine to `running`

Unload path:

1. Pause emulation
2. Drain stream outputs
3. Detach dependent modules
4. Run module deinit hooks
5. Release allocated regions
6. Update runtime catalog state
