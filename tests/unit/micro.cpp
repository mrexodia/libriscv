#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <libriscv/machine.hpp>
#include <libriscv/debug.hpp>
extern std::vector<uint8_t> build_and_load(const std::string& code,
	const std::string& args = "-O2 -static", bool cpp = false);
static constexpr uint32_t MAX_CYCLES = 5'000;
static const std::vector<uint8_t> empty;
using namespace riscv;

TEST_CASE("Run exactly X instructions", "[Micro]")
{
	Machine<RISCV32> machine;

	std::array<uint32_t, 3> my_program{
		0x29a00513, //        li      a0,666
		0x05d00893, //        li      a7,93
		0xffdff06f, //        jr      -4
	};

	const uint32_t dst = 0x1000;
	machine.copy_to_guest(dst, &my_program[0], sizeof(my_program));
	machine.memory.set_page_attr(dst, riscv::Page::size(), {
		.read = false,
		.write = false,
		.exec = true
	});
	machine.cpu.jump(dst);

	// Step instruction by instruction using
	// a debugger.
	riscv::DebugMachine debugger{machine};
	debugger.verbose_instructions = true;

	debugger.simulate(3);
	REQUIRE(machine.cpu.reg(REG_ARG0) == 666);
	REQUIRE(machine.cpu.reg(REG_ARG7) == 93);
	REQUIRE(machine.instruction_counter() == 3);

	machine.cpu.reg(REG_ARG7) = 0;

	debugger.simulate(2);
	REQUIRE(machine.instruction_counter() == 5);
	REQUIRE(machine.cpu.reg(REG_ARG7) == 93);

	// Reset CPU registers and counter
	machine.cpu.registers() = {};
	machine.cpu.jump(dst);

	// Normal simulation
	machine.simulate<false>(2, 0u);
	REQUIRE(machine.instruction_counter() == 3);
	REQUIRE(machine.cpu.reg(REG_ARG7) == 93);

	machine.cpu.reg(REG_ARG7) = 0;

	machine.simulate<false>(2, machine.instruction_counter());
	REQUIRE(machine.instruction_counter() == 5);
	REQUIRE(machine.cpu.reg(REG_ARG7) == 93);
}

TEST_CASE("Crashing payload #1", "[Micro]")
{
	static constexpr uint32_t MAX_CYCLES = 5'000;

	// Print-out from GDB
	const char elf[] = "\177ELF\002\002\002\216\002\002\002\002\002\002\f\004\002\000\363\000z\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\a\000\000\000\000\000\000\000\004\000\000\000\000\000\000\000\000\000\256\377\377\377\373\377\377\000\000`\260\000\217\377P\377\377\377\377\377\377\017\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\002\002\002\002\002\000\374\376\375\377\f\377\377\327\000\000\000\000\000\366\000\000\000\000\000\000\000\000\000\000\000\374~EL\271\372\001\002\213\375\375\375\375\002\002\377\004\002\000\363\000\000\000\000\000\000\000\001\000\000\000\221\377d\000\374\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000~\000\000\000\000\000\000\000\000\374\257\000\000\377\001\000\000\000\000\000\000\020\000\000\000\000\000\000\000\000\370\377\377\b\001\020b\000>>>>>>>>>>>>\000\002\002\002\002\002\002\002\002\002\002\002\002\002\002\002\263\002\002\002\002\002\002\002\000\006\000\005\000\002\002\002\002\002\002\002\002\002\303\303\303\303\303\303\323\303\303\303\303\002\002\023\002\002\263E\000\002\002\002\361\361\361\361\361\361\361\361\361\361\361\361\361\361\361\361\361\361\361\361\002\002\002\002\002\002\002\231\231\231\231\000\000\377\377\377\377\377\377\377\f\377\377\f\370\231LF\002z\002\377\377\000\002\000\363\000\177ELF\200\000\000\000\000\000\000\000\002";
	std::string_view bin { elf, sizeof(elf)-1 };

	const MachineOptions<8> options {
		.allow_write_exec_segment = true,
		.use_memory_arena = false
	};
	try {
		Machine<8> machine { bin, options };
		machine.on_unhandled_syscall = [] (auto&, size_t) {};
		machine.simulate(MAX_CYCLES);
	} catch (const std::exception& e) {
		//printf(">>> Exception: %s\n", e.what());
	}
}

TEST_CASE("Crashing payload #2", "[Micro]")
{
	static const uint8_t crash[] = {
		0x80, 0xbf, 0x25, 0x00, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x25,
		0x10, 0x25, 0x00, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x30,
		0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x63, 0x01,
		0x41, 0x00, 0xf4, 0x1a, 0x00, 0xcb, 0xcb, 0xcb, 0xda, 0xda, 0xda, 0xda,
		0xda, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
		0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d,
		0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x1d, 0x35, 0x35, 0x35,
		0x34, 0x3f, 0x35, 0x35, 0x55, 0xf5
	};

	constexpr uint32_t S = 0x1000;
	constexpr uint32_t V = 0x2000;

	riscv::Machine<RISCV64> machine { empty };
	bool exception_thrown = false;
	try
	{
		machine.memory.set_page_attr(S, 0x1000, {.read = true, .write = true});
		machine.memory.set_page_attr(V, 0x1000, {.read = true, .exec = true});
		machine.on_unhandled_syscall = [] (auto&, size_t) {};
		machine.cpu.init_execute_area(crash, V, sizeof(crash));

		machine.cpu.jump(V);

		machine.simulate(MAX_CYCLES);
	} catch (const std::exception& e) {
		fprintf(stderr, ">>> Exception: %s\n", e.what());
		exception_thrown = true;
	}

	REQUIRE(exception_thrown);
}

TEST_CASE("Crashing payload #3", "[Micro]")
{
	static const uint8_t crash[] = {
		0x17, 0x00, 0x17, 0x60, 0x60, 0x60, 0x60, 0xff, 0x60, 0x60, 0x60, 0x60,
		0x60, 0x60, 0x1c, 0xff, 0xe3, 0xff, 0xff, 0xff
	};

	constexpr uint32_t S = 0x1000;
	constexpr uint32_t V = 0x2000;

	riscv::Machine<RISCV64> machine { empty };
	bool exception_thrown = false;
	try
	{
		machine.memory.set_page_attr(S, 0x1000, {.read = true, .write = true});
		machine.memory.set_page_attr(V, 0x1000, {.read = true, .exec = true});
		machine.on_unhandled_syscall = [] (auto&, size_t) {};
		machine.cpu.init_execute_area(crash, V, sizeof(crash));

		machine.cpu.jump(V);

		machine.simulate(MAX_CYCLES);
	} catch (const std::exception& e) {
		fprintf(stderr, ">>> Exception: %s\n", e.what());
		exception_thrown = true;
	}

	REQUIRE(exception_thrown);
}

TEST_CASE("Crashing payload #4", "[Micro]")
{
	static const uint8_t crash[] = {
		0xb7, 0xa1, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3,
		0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0x41, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1, 0x8f, 0x2f,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b,
		0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d,
		0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x10, 0x10, 0xa1,
		0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64,
		0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0x41, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64,
		0x80, 0x1b, 0x1b, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d,
		0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03,
		0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x10, 0xa0, 0x00, 0x10, 0x2f, 0x41, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa0, 0xfe, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1,
		0x00, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63,
		0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26,
		0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64,
		0x80, 0x1b, 0x1b, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x3f, 0xa1, 0x00, 0x00, 0x48, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d,
		0x10, 0x00, 0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07,
		0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27,
		0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d,
		0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d,
		0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03,
		0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1,
		0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x3a, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x40, 0x00, 0xa3,
		0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x00, 0xa3,
		0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3,
		0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x10, 0xa0, 0x00,
		0x10, 0x2f, 0x41, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa0, 0xfe, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3,
		0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x0a, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1,
		0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64,
		0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31,
		0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x10,
		0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x10, 0x00, 0x00, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b,
		0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0xa1, 0x2f, 0x10, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10,
		0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07,
		0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27,
		0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27,
		0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b,
		0x1b, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f,
		0xa1, 0x00, 0x00, 0x48, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x10, 0x00,
		0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64,
		0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1,
		0x00, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63,
		0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26,
		0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1, 0x8f, 0x2f,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31,
		0x3a, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0x00, 0x10, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0x10, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x10, 0xa0, 0x00, 0x10, 0x2f,
		0x41, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa0, 0xfe,
		0x00, 0x40, 0x00, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63, 0x76, 0x31,
		0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x10,
		0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x10, 0x00, 0x00, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b,
		0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0xa1, 0x2f, 0x10, 0x00, 0x00, 0x40,
		0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10,
		0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07,
		0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27,
		0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27,
		0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b,
		0x1b, 0x10, 0x10, 0xa1, 0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f,
		0xa1, 0x00, 0x00, 0x48, 0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x10, 0x00,
		0x00, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x63, 0x76, 0x31, 0x36, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64,
		0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b,
		0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80,
		0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x27, 0x1b, 0x1b, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x00,
		0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00,
		0x10, 0x07, 0xf4, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26,
		0x2f, 0x27, 0x1b, 0x1b, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00,
		0x10, 0x2f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0xa3, 0x00, 0x10, 0x2f, 0x10,
		0x2f, 0xa1, 0x00, 0x10, 0x2f, 0xa1, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x2f,
		0xa1, 0x00, 0x00, 0x40, 0x00, 0x2d, 0x00, 0xa1, 0x00, 0x10, 0x10, 0xa1,
		0x8f, 0x2f, 0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40,
		0x00, 0x10, 0xa3, 0x00, 0x10, 0x3f, 0xa1, 0x00, 0x00, 0x40, 0x00, 0x63,
		0x76, 0x31, 0x3a, 0x4d, 0x61, 0x63, 0x9b, 0x07, 0xf4, 0x64, 0x80, 0x1b,
		0x1b, 0x1b, 0x03, 0x03, 0x2d, 0x26, 0x2f, 0x00, 0x00, 0x40, 0x00, 0x00,
		0x00, 0x64, 0x80, 0x1b, 0x1b, 0x1b, 0x03, 0x03, 0x33, 0x2c, 0x01, 0x13,
		0x21, 0xe1, 0x29, 0xa9, 0x2a, 0x02, 0xfe, 0xf8, 0xf5, 0x33, 0x30, 0xdf
	};

	constexpr uint32_t S = 0x1000;
	constexpr uint32_t V = 0x2000;

	// 10: Test 4, exec = 0x2000 -> 0x2A5C
	// 10: Branch 0x21FA >= 0x1950 (decoder=0x5290000168b8)
	// 10: [0x21FA] 1b1b272f SC.W [SR6], A7 res=A4
	// What happens:
	// There is an end-of-block due to a limit at 0x21FA
	// After this, PC is no longer incremented.
	riscv::Machine<RISCV64> machine { empty, {.use_memory_arena = false} };
	bool exception_thrown = false;
	try
	{
		machine.memory.set_page_attr(S, 0x1000, {.read = true, .write = true});
		machine.memory.set_page_attr(V, 0x1000, {.read = true, .exec = true});
		machine.on_unhandled_syscall = [] (auto&, size_t) {};
		machine.cpu.init_execute_area(crash, V, sizeof(crash));

		machine.cpu.jump(V);

		machine.simulate(MAX_CYCLES);
		//DebugMachine debugger { machine };
		//debugger.verbose_instructions = true;
		//debugger.simulate(MAX_CYCLES);

	} catch (const std::exception& e) {
		fprintf(stderr, ">>> Exception: %s\n", e.what());
		exception_thrown = true;
	}

	REQUIRE(exception_thrown);
}

TEST_CASE("Crashing payload #5", "[Micro]")
{
	// This type of crash starts at 0x100 and ends at 0x11A
	static const char crash1[] =
		"\177ELF\001AL\213\213\213\213\000\000\000\377\372\002\000\363\000\000\001\000F\000\001\000\000\000\000\000\000\000\001\000\000\000\000\000\000\001\000\000\004\004\000\001\000\004\000\000\000\000\063\000\002\000\363\000\364F5Fb\001\000\000\000\245\000\000\000\000\000\000\000\000\000\000\000\272\001\000\000\377\377~\377\243\333>\371\020\000\000\000.\000\000@\000\001\377\377\377\377\000\001\200\367\377\000\000\004\000\000\035\000\000\000\377\377\377\377\377\377\377\016\000\377.t\363\000\364F\306Fb\001\000\000\000\000\000\000\000L\000\027\000\000\000\000\000\000\000\002\364\000\363\000F\306FbF\000\000\000.\000\000@\000\005\b\000\000\000\000Fb\001\000\000\000\000\000\000\000L\000\227\000\334\000\000\000\000\000\003L\375\000\000\000\000\000\000,\000\002\000\363\000\000\001\000F\000\001\000\000`\000\000\000\000\000\000\001\000]\000\000\000\001\000\000\000\004\000\001\000\004\000\000\000\000\060\000\002\000\363\000\000\000\000\000\002\000\363\000\364F\306F\000b\000\377\000\000\000\000\000L\000\227\376\000\000\000\000\000\000\002\364\000\363\000F\306FbF\000\000\000.\000\000@\000\000\b\000\000\000(\000Fb\001\000\000\000\000\000\000\000L\000\227\000\334\000\000\000\000\000\003L\375\000\000\000\000\000\000,\000\002\000\363\000\000\001\000F\000\001\000\000`\000\000\000\000\000\001\000\000\000\000\000\000\001\000\000\000\004\000\001\000\004\000\000\000\000\060\000\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r1\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\r\033\002\000\000\000\000\000\000\000\000.\000\000@\000\177ELF\177\001\000b\020\002\261\236\264EL\000!\000\267\267\267XXXXXXXXXXXXXXXXXXXXX\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\220\020\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\363\000\000\000.\000\000@\000\001\377\377\377\377\000\001\200\367\377\000\000\004\000\000\035\000\000\000\377\377\377\377\377\377\377\016\000\377.t\363\000\364F\306Fb\001\000\000\000\000\000\000\000L\000\027\000\000\000\000\000\000\000\002\364\000\363\000F\306FbF\000\363\000\364F5Fb\001\000\000\000\245\000\000\000\000\000\000\000\000\000\000\000\272\001\000\000\377\377~\377\243\333>\371\020\000\000\000.\000\000@\000\001\377\377\377\377\000\001\200\367\377\000\000\004\000\000\035\000\000\000\377\377\377\377\377\377\377\016\000\377.t\363\000\364F\306Fb\001\000\000\000\000\000\000\000L\000\000\000.\000\000@\000\005\b\000\000\000\000Fb\001\000\000\000\000\000\000\000L\000\227\000\334\000\000gggggggggL\000\027\000\000\000\000\000;\000\000\003\364\000\363\000F\306FbF\000\000\000.\000\000@\000\005\b\000\363\000\364F0F";
	static const char crash2[] =
		"\177ELF\001\000\000\000\000\377\002\000\001\317\000`\003\000\363\000\000\000\002\000\000\001\000\000,\000\000\000 \000\000\000\000\317\000\000\000\000\001t\001\000\000\000\001\000\000\000D\000\000\000\000\000\000\000\326\000\000\000\000\000\000\000\a\a\a\006\002\000\000\000\a\a\a\377\377\000\000\377\002\000\001\317\000`\003\000\363\000\177E\272FL\000\000\000\000\000\000\000\001&\000\000\000\000\000\000\000\001\371\371\371\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\216\230\371\371\371\371\230\033\033\033\033;\000\000\000\000\000\000\000\326\000\000\000\000[\230\230\033\033\033\033\033\033\033\033\033\033\033\033\033\033\033\001\000\000\000\000\000\000\000\033\006\006\006\t\371\371\371\371\371\376\371\371\371\371[\371\317@`\002\000\363\244\000\000\000\000\000\001\020\000\000,\000\000\000 \000\000\000\000\317\000\000\377\200";
	const MachineOptions<RISCV32> options {
		.allow_write_exec_segment = true,
		.use_memory_arena = false
	};

	bool exception_thrown = false;
	try
	{
		riscv::Machine<RISCV32> machine { std::string_view(crash1, sizeof(crash1)), options };
		machine.on_unhandled_syscall = [] (auto&, size_t) {};
		machine.simulate(MAX_CYCLES);
	} catch (const std::exception& e) {
		fprintf(stderr, ">>> Exception: %s\n", e.what());
		exception_thrown = true;
	}
	REQUIRE(exception_thrown);

	exception_thrown = false;
	try
	{
		riscv::Machine<RISCV32> machine { std::string_view(crash2, sizeof(crash2)), options };
		machine.on_unhandled_syscall = [] (auto&, size_t) {};
		machine.simulate(MAX_CYCLES);

	} catch (const std::exception& e) {
		fprintf(stderr, ">>> Exception: %s\n", e.what());
		exception_thrown = true;
	}
	REQUIRE(exception_thrown);
}
