# Building for newlib

Newlib is a libc for embedded systems that also has C++ exceptions support. You can build this cross-compiler yourself like so:

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv --with-arch=rv32g --with-abi=ilp32d
make
```
This will build a newlib cross-compiler with C++ exception support. The ABI is ilp32d, which is for 32-bit and 64-bit floating-point instruction set support. It is much faster than software implementations of binary IEEE floating-point arithmetic.

Note that if you want a full glibc cross-compiler instead, simply appending `linux` to the make command will suffice, like so: `make linux`. Glibc produces larger binaries but has more features, like sockets and threads.

```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv --with-arch=rv64g --with-abi=lp64d
make
```
The incantation for 64-bit RISC-V. Not enabling the C-extension for compressed instructions results in faster emulation.

The last step is to add your compiler to PATH so that it becomes visible to build systems. So, add this at the bottom of your `.bashrc` file in the home (~) directory:

```
export PATH=$PATH:$HOME/riscv/bin
```