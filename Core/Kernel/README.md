# Kernel runtime foundation

M9 adds a platform-neutral process and thread runtime above the M7 loader/memory
and M8 CPU layers:

`Process -> Thread -> Scheduler -> CpuExecutor -> GuestMemory`

The syscall dispatcher owns handler registration and the initial `ProcessExit`
service. The exception boundary translates CPU faults into runtime exceptions so
the CPU executor does not terminate processes itself.

The current scheduler runs ready threads sequentially. It is intentionally not a
preemptive or time-sliced implementation yet.
