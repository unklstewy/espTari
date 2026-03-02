# 2. Non-negotiable architecture constraints

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 02

## 2. Non-negotiable architecture constraints

1. **No local display pipeline dependency**
   - Do not use MIPI DSI as the default display path.
   - Video output is produced by the emulation core and exposed via streaming APIs for browser clients.

2. **No frontend coupling in this phase**
   - Deliver backend APIs, control plane, and streaming plane only.

3. **SD-card as media and module storage authority**
   - Disk images, ROM images, cartridge images, and EBIN component binaries are stored and loaded from SD-card paths.

4. **Dynamic EBIN component loading/unloading**
   - Machine components are deployable as custom EBIN binaries.
   - EBINs are loaded into engine runtime memory and can be unloaded/reloaded to switch machine profiles or component revisions.

5. **Remote file management required**
   - Browser-accessible upload/download/list/delete/move APIs for SD-card assets and EBINs.

6. **Full runtime introspection APIs required**
   - Bus and memory-map call tracing APIs.
   - Register inspection APIs with streamed updates.

7. **Input translation APIs required**
  - Engine must accept physical keyboard/mouse/game-controller input events.
  - Engine must translate host input events into machine-accurate virtual inputs for the active machine profile.
  - Input ingress source for this phase is browser session clients.
  - Engine must support input enable/disable and mouse-capture policy controls.

8. **Machine state save/restore lifecycle required**
  - Engine must support machine state saving and snapshot persistence.
  - Engine must support suspend-and-save and restore-and-resume workflows.
  - Saved state must include profile and ABI compatibility metadata.

9. **Hard runtime performance metrics required**
  - Input-device end-to-end latency target: 50 ms or less.
  - Runtime jitter target: less than 30 ms.
  - Dropped-frame rate target: less than 1 percent.

10. **Debug clock-control required**
  - Engine must support realtime, slow-motion, and single-step execution modes for debugging.
  - Debug execution must remain observable through opcode/bus/register tracing APIs.

---
