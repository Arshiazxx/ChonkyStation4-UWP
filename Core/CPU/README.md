# CPU foundation

M8 adds a platform-neutral CPU execution harness. `CpuState` models the
x86-64-shaped register file, RIP, RSP, arithmetic flags, and execution state.
`CpuExecutor` fetches synthetic instructions from `GuestMemory`, decodes them,
dispatches execution, and stops on HALT, an access/decode fault, or an
instruction limit.

The supported validation encoding is intentionally small: NOP, MOV immediate,
ADD immediate, SUB immediate, LOAD, STORE, and HALT. It is not the PS4 native
x86-64 decoder and does not execute real eboot code.
