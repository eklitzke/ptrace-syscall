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

    call simple_method()

from GDB while attached to a process that was attached while in `syscall`. GDB
also caused a segfault in the same way as my original code did.

I am currently investigating whether this is due to a bug in my other program
(which is much larger and more complex -- so not unlikely), a difference related
to different kernel versions, some complexity of `sigaction()` in the tracee
process, etc.

For posterity's sake, I will update this project if I am able to identify the
circumstances where this code fails.
