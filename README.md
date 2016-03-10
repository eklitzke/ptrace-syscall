# Original Goal

This is supposed to demonstrate a problem I am having when trying to call
userspace code when `ptrace` attaching to a process that is currently blocked
making a syscall. To use:

    $ make
    $ ./target  # first line prints pid and funcptr

Now in a second terminal:

    $ ./tracer <pid> <funcptr>

I recommend copying exactly the pid and funcptr printed by `./target`.

The funny thing about this code is that I wrote it as a simplified test case to
demonstrate where `ptrace()` was failing (when interacting with `syscall`), but
if you try the demo in this code it works just fine. In fact, the function
called by the `funcptr` really will be called (assuming you passed the correct
argument) and Linux is even so magical that the `select(2)` will not even return
with `EINTR`, meaning that the whole process is invisible to the tracee.

Originally I was even able to reproduce the issue from GDB, i.e. by trying to
issue a statement like

    call MethodIExpectedToWork()

from GDB while attached to a process that was attached while in `syscall`. GDB
also caused a segfault in the same way as my original code did.

I am currently investigating whether this is due to a bug in my other program
(which is much larger and more complex -- so not unlikely), a difference related
to different kernel versions, some complexity of `sigaction()` in the tracee
process, etc.

For posterity's sake, I will update this project if I am able to identify the
circumstances where this code fails.

## Conclusion: What Actually Happened

The function I was trying to invoke was `PyEval_GetFrame()` which gets the
current Python frame. This method is implemented by calling the unsafe method
`PyThreadState_GET()` which does no error checking -- it assumes that you are
making the call from a state in which there is a current/valid threadstate in
Python. There is another method called `PyThreadState_Get()` which does do error
checking. The current frame is a field in the `PyThreadState` struct.

The method `PyThreadState_Get()` is implemented in `Python/pystate.c` and
accesses the [TLS](https://en.wikipedia.org/wiki/Thread-local_storage) variable
`_PyThreadStateCurrent`.The method `PyThreadState_Get()` checks if this variable
is NULL. If the variable is NULL the program is aborted, otherwise the value of
`_PyThreadStateCurrent` is returned.

The method `PyThreadState_GET()` is a macro implemented in `Include/pystate.h`
that simply evaluates to `_PyThreadStateCurrent`. Meaning if that if the
variable is NULL and you try to access a field on it you will immediately
segfault.

When you run Python as a REPL, it initializes itself in a very strange way --
totally differently from how it works when you run a Python script. When invoked
as a REPL it basically just enters a
[readline](https://cnswww.cns.cwru.edu/php/chet/readline/rltop.html) loop that
uses `select()` to read characters from stdin, and then does the equivalent of
calling `eval` on each line you enter. So at any given time, when the REPL is
idle, there is actually no current threadstate and no current frame. If you want
to know more details I recommend looking at the GDB backtrace of a REPL
interpreter vs a script and you can see how they're initialized very
differently.

This was the root of my problem -- I was testing my code against a bare `python`
REPL and not an actual Python script.

Because of how GCC optimizes the Python code, literally the first instruction of
`PyEval_GetFrame` accesses `_PyThreadStateCurrent`. In fact, the entire method
disassembles into this:

    Dump of assembler code for function PyEval_GetFrame:
       0x000000000055b350 <+0>:     mov    0x4422f9(%rip),%rdi        # 0x99d650 <_PyThreadState_Current>
       0x000000000055b357 <+7>:     mov    0x441512(%rip),%rax        # 0x99c870 <_PyThreadState_GetFrame>
       0x000000000055b35e <+14>:    jmpq   *%rax

So if you try to call/jump to `PyEval_GetFrame` the SEGFAULT happens immediately
on the first instruction; even if you try to single step you'll immediately
fail. Mystery solved.

P.S. If you're familiar with x86 assembler you'll notice that the disassembly
for this function looks incredibly strange -- not at all what you'd expect a
compiler to emit. The reason the disassembly for this function looks so strange
is because of Python's TLS implementation. Modern compilers have supported TLS
natively for years now. I have no idea how it's implemented in a compiler like
GCC, Clang, or MSVC++, but presumably they use a lot of magic and weird
instrinsics to make TLS work super fast. However, when Python was written in the
90s not all compilers supported TLS or supported it well, and therefore Python
doesn't use any of the native compiler-provided TLS implementations. The version
implemented by Python uses some magical code in `Python/pystate.c` which is
ultimately called by `Py_Initialize()` and `Py_Finalize()` (which are called
during the paths that create and destroy Python threads). It works by basically
ensuring that these methods alter a variable called `autoTLSkey` via some magic
in `Python/thread.c`. Writing your own TLS implementation in the 90s might have
been reasonable, but is sort of crazy now. There are a bunch of weird Python C
anachronisms like this (ahem, Python memory management code) I've found that I'd
like to write about in a blog post at some point.

P.P.S. From the perspective of a tracer you can detect between the two states by
inserting code like:

```gas
call *%rax
int3
```

You would then use `ptrace(PTRACE_SETREGS, ...)` to set `%rax` to
`PyThreadState_Get()`. Then you call `ptrace(PTRACE_CONT, ...)` and wait for the
process to TRAP or ABRT via a call to one of the "wait" family of functions. If
the process stopped with `SIGABRT` then you know you're trying to trace a REPL
and you can handle that appopriately (even if that just means printing "we don't
support REPLs" to stderr"). If instead the process stopped with `SIGTRAP` then
you can access the frame pointer by using `ptrace(PTRACE_PEEKTEXT, ...)` at the
correct offset from the `PyThreadState` struct.

In either case, if you write your code correctly you can set it up so that the
original process text and registers are restored so that the normal fatal action
of `PyThreadState_Get()` raising `SIGABRT` will not actually terminate the
program.
