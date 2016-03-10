This demonstrates a problem I am having when trying to call userspace code when
`ptrace` attaching to a process that is currently in the `syscall` instruction.
To use:

    $ make
    $ ./target  # first two arguments will be pid and func ptr

Now in a second terminal:

    $ ./tracer <pid> <funcptr>

I recommend copying exactly the pid and funcptr printed by `./target`.
