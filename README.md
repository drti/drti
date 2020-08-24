# The Dynamic Runtime Inlining (DRTI) project

With this project it is possible to take the output from an
[LLVM](http://llvm.org/) compiler, such as
[clang](https://clang.llvm.org/), and allow selected parts of the code
to recompile themselves at runtime. This can inline function calls
across shared-object boundaries as well as calls made via function
pointers. In both of those cases the targets of the calls are
generally known only at runtime, hence "Runtime Inlining".

This is different to LLVM's [Link Time
Optimizations](https://www.llvm.org/docs/LinkTimeOptimization.html)
and can inline cases that LTO doesn't handle. It also **doesn't**
involve an LLVM "bitcode" **interpreter**; it runs native machine code
generated by Ahead of Time (AOT) compilation that can make calls to a
Just in Time (JIT) runtime compiler.

The basic concept is to get the normal compiler front end like clang
to emit its LLVM Intermediate Representation (IR) as a bitcode file,
then run a custom LLVM pass over this to inject DRTI code at the
appropriate places. Importantly the custom pass also embeds the
**original** bitcode as a binary string so that it is available at
runtime for recompilation. The command-line tool llc compiles the
adapted bitcode into a native object file.

Although the concept is fairly simple there were (and still are)
several challenges in its implementation. Currently the project is a
proof of concept, demonstrating that the idea can work, but not likely
to improve the speed of any real-world applications.


## Supported Platforms

For now the code only works on Linux on the x86-64 architecture. It is
dependent on version 9.0.1 of the LLVM libraries.