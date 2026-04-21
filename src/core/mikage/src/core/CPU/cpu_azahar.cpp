#include "../../../include/cpu_interpreter.hpp"

#include "../../../include/emulator.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "../../../third_party/azahar_cpu/arm/dyncom/arm_dyncom.h"
#include "../../../third_party/azahar_cpu/arm/compat/core/core.h"
#include "../../../third_party/azahar_cpu/arm/compat/core/core_timing.h"
#include "../../../third_party/azahar_cpu/arm/compat/core/memory.h"
#include <functional>

namespace {
class AzaharMemoryAdapter final : public Memory::MemorySystem {
  public:
	explicit AzaharMemoryAdapter(Memory& memory) : mem(memory) {}

	std::uint8_t Read8(std::uint32_t address) const override { return mem.read8(address); }
	std::uint16_t Read16(std::uint32_t address) const override { return mem.read16(address); }
	std::uint32_t Read32(std::uint32_t address) const override { return mem.read32(address); }
	std::uint64_t Read64(std::uint32_t address) const override { return mem.read64(address); }

	void Write8(std::uint32_t address, std::uint8_t value) override { mem.write8(address, value); }
	void Write16(std::uint32_t address, std::uint16_t value) override { mem.write16(address, value); }
	void Write32(std::uint32_t address, std::uint32_t value) override { mem.write32(address, value); }
	void Write64(std::uint32_t address, std::uint64_t value) override { mem.write64(address, value); }

  private:
	Memory& mem;
};

class AzaharTimerAdapter final : public Core::Timing::Timer {
  public:
	explicit AzaharTimerAdapter(Scheduler& scheduler) : scheduler(scheduler) {}

	int64_t GetDowncount() const override {
		if (scheduler.nextTimestamp <= scheduler.currentTimestamp) {
			return 0;
		}
		return static_cast<int64_t>(scheduler.nextTimestamp - scheduler.currentTimestamp);
	}

	void AddTicks(uint64_t ticks) override { scheduler.currentTimestamp += ticks; }

  private:
	Scheduler& scheduler;
};

class AzaharSystemAdapter final : public Core::System {
  public:
	explicit AzaharSystemAdapter(CPU& cpu) : cpu(cpu) {}

	void AddTicks(std::uint64_t ticks) override { cpu.addTicks(ticks); }

	void CallSVC(std::uint32_t svc) override {
		if (callSVC) {
			callSVC(svc);
		}
	}

	std::function<void(std::uint32_t)> callSVC{};

  private:
	CPU& cpu;
};
}  // namespace

struct CPU::AzaharBackend {
	AzaharMemoryAdapter memory;
	std::shared_ptr<AzaharTimerAdapter> timer;
	AzaharSystemAdapter system;
	Core::ARM_DynCom core;

	AzaharBackend(CPU& cpu)
		: memory(cpu.mem),
		  timer(std::make_shared<AzaharTimerAdapter>(cpu.scheduler)),
		  system(cpu),
		  core(system, memory, PrivilegeMode::USER32MODE, 0, timer) {
		system.callSVC = [this, &cpu](std::uint32_t svc) {
			syncFromBackend(cpu);
			cpu.kernel.serviceSVC(svc);
			syncToBackend(cpu);
		};
	}

	void syncFromBackend(CPU& cpu) {
		for (int i = 0; i < 16; i++) {
			cpu.gprs[static_cast<size_t>(i)] = core.GetReg(i);
		}
		for (int i = 0; i < 64; i++) {
			cpu.extRegs[static_cast<size_t>(i)] = core.GetVFPReg(i);
		}
		cpu.cpsr = core.GetCPSR();
		cpu.fpscr = core.GetVFPSystemReg(VFPSystemRegister::VFP_FPSCR);
		cpu.tlsBase = core.GetCP15Register(CP15Register::CP15_THREAD_UPRW);
	}

	void syncToBackend(const CPU& cpu) {
		for (int i = 0; i < 16; i++) {
			core.SetReg(i, cpu.gprs[static_cast<size_t>(i)]);
		}
		for (int i = 0; i < 64; i++) {
			core.SetVFPReg(i, cpu.extRegs[static_cast<size_t>(i)]);
		}
		core.SetCPSR(cpu.cpsr);
		core.SetVFPSystemReg(VFPSystemRegister::VFP_FPSCR, cpu.fpscr);
		core.SetCP15Register(CP15Register::CP15_THREAD_UPRW, cpu.tlsBase);
	}
};

CPU::CPU(Memory& mem, Kernel& kernel, Emulator& emu)
	: mem(mem), scheduler(emu.getScheduler()), kernel(kernel), emu(emu), azahar(std::make_unique<AzaharBackend>(*this)) {
	mem.setCPUTicks(getTicksRef());
	reset();
}

void CPU::reset() {
	gprs.fill(0);
	extRegs.fill(0);
	cpsr = CPSR::UserMode;
	fpscr = FPSCR::MainThreadDefault;
	tlsBase = VirtualAddrs::TLSBase;
	bridgeState.reset();
	itCond = 0;
	itMask = 0;

	azahar->core.ClearInstructionCache();
	azahar->syncToBackend(*this);
}

void CPU::runFrame() {
	emu.frameDone = false;
	azahar->syncToBackend(*this);

	while (!emu.frameDone) {
		u64 ticksLeft = scheduler.nextTimestamp - scheduler.currentTimestamp;
		while (!emu.frameDone && ticksLeft > 0) {
			const u64 before = scheduler.currentTimestamp;
			azahar->core.Step();
			const u64 consumed = scheduler.currentTimestamp > before ? scheduler.currentTimestamp - before : 1;
			ticksLeft = consumed >= ticksLeft ? 0 : ticksLeft - consumed;
		}
		emu.pollScheduler();
	}

	azahar->syncFromBackend(*this);
}
