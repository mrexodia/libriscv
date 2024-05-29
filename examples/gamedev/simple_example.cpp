#include <fstream>
#include <iostream>
#include <libriscv/machine.hpp>
using namespace riscv;

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::cout << argv[0] << ": [program file] [arguments ...]" << std::endl;
		return -1;
	}

	// Read the RISC-V program into a std::vector:
	std::ifstream stream(argv[1], std::ios::in | std::ios::binary);
	if (!stream) {
		std::cout << argv[1] << ": File not found?" << std::endl;
		return -1;
	}
	const std::vector<uint8_t> binary(
		(std::istreambuf_iterator<char>(stream)),
		std::istreambuf_iterator<char>());

	// Create a new 64-bit RISC-V machine
	Machine<RISCV64> machine{binary, {.memory_max = 64UL << 20}};

	// Use string vector as arguments to the RISC-V program
	machine.setup_linux(
		{"micro", "Hello World!"},
		{"LC_TYPE=C", "LC_ALL=C", "USER=root"});
	machine.setup_linux_syscalls(false, false);

	try {
		// Run through main(), but timeout after 32mn instructions
		machine.simulate(32'000'000ull);
	} catch (const std::exception& e) {
		std::cout << "Program error: " << e.what() << std::endl;
		return -1;
	}

	machine.vmcall("my_function", "Hello Sandboxed World!");

	std::cout << "Program exited with status: " << machine.return_value<int>() << std::endl;
	return 0;
}