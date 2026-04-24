#include "../../../include/kernel/kernel.hpp"

#include <cassert>
#include <limits>

#include "../../../include/cpu.hpp"
#include "../../../include/kernel/kernel_types.hpp"

Kernel::Kernel(CPU& cpu, Memory& mem, GPU& gpu, const EmulatorConfig& config)
	: cpu(cpu), regs(cpu.regs()), mem(mem), handleCounter(0), serviceManager(regs, mem, gpu, currentProcess, *this, config), fcramManager(mem) {
	objects.reserve(512);  // Make room for a few objects to avoid further memory allocs later
	mutexHandles.reserve(8);
	portHandles.reserve(32);
	threadIndices.reserve(appResourceLimits.maxThreads);

	for (int i = 0; i < threads.size(); i++) {
		Thread& t = threads[i];

		t.index = i;
		t.tlsBase = VirtualAddrs::TLSBase + i * VirtualAddrs::TLSSize;
		t.status = ThreadStatus::Dead;
		t.waitList.clear();
		t.waitList.reserve(10);  // Reserve some space for the wait list to avoid further memory allocs later
		// The state below isn't necessary to initialize but we do it anyways out of caution
		t.outPointer = 0;
		t.waitAll = false;
	}

	setVersion(1, 69);
}

void Kernel::serviceSVC(u32 svc) {
	switch (svc) {
		case 0x00:
			Helpers::warn("Reference SVC stub not yet implemented: %X @ %08X", svc, regs[15]);
			regs[0] = Result::OS::NotImplemented;
			break;
		case 0x01: controlMemory(); break;
		case 0x02: queryMemory(); break;
		case 0x03: exitThread(); break;  // ExitProcess (single-process model fallback)
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x0D:
		case 0x0E:
		case 0x10:
		case 0x12:
			Helpers::warn("Reference SVC stub not yet implemented: %X @ %08X", svc, regs[15]);
			regs[0] = Result::OS::NotImplemented;
			break;
		case 0x08: createThread(); break;
		case 0x09: exitThread(); break;
		case 0x0A: svcSleepThread(); break;
		case 0x0B: getThreadPriority(); break;
		case 0x0C: setThreadPriority(); break;
		case 0x0F: getThreadIdealProcessor(); break;
		case 0x11: getCurrentProcessorNumber(); break;
		case 0x13: svcCreateMutex(); break;
		case 0x14: svcReleaseMutex(); break;
		case 0x15: svcCreateSemaphore(); break;
		case 0x16: svcReleaseSemaphore(); break;
		case 0x17: svcCreateEvent(); break;
		case 0x18: svcSignalEvent(); break;
		case 0x19: svcClearEvent(); break;
		case 0x1A: svcCreateTimer(); break;
		case 0x1B: svcSetTimer(); break;
		case 0x1C: svcCancelTimer(); break;
		case 0x1D: svcClearTimer(); break;
		case 0x1E: createMemoryBlock(); break;
		case 0x1F: mapMemoryBlock(); break;
		case 0x20: unmapMemoryBlock(); break;
		case 0x21: createAddressArbiter(); break;
		case 0x22: arbitrateAddress(); break;
		case 0x23: svcCloseHandle(); break;
		case 0x24: waitSynchronization1(); break;
		case 0x25: waitSynchronizationN(); break;
		case 0x26: signalAndWait(); break;
		case 0x27: duplicateHandle(); break;
		case 0x28: getSystemTick(); break;
		case 0x29: getHandleInfo(); break;
		case 0x2A: getSystemInfo(); break;
		case 0x2B: getProcessInfo(); break;
		case 0x2C: getThreadInfo(); break;
		case 0x2D: connectToPort(); break;
		case 0x2E:
		case 0x2F:
		case 0x30:
		case 0x31:
		case 0x32: sendSyncRequest(); break;
		case 0x33: openProcess(); break;
		case 0x34: openThread(); break;
		case 0x35: getProcessID(); break;
		case 0x36: getProcessIDOfThread(); break;
		case 0x37: getThreadID(); break;
		case 0x38: getResourceLimit(); break;
		case 0x39: getResourceLimitLimitValues(); break;
		case 0x3A: getResourceLimitCurrentValues(); break;
		case 0x3B: getThreadContext(); break;
		case 0x3D: outputDebugString(); break;
		case 0x3C:
		case 0x3E:
		case 0x3F:
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
			Helpers::warn("Reference SVC stub not yet implemented: %X @ %08X", svc, regs[15]);
			regs[0] = Result::OS::NotImplemented;
			break;
		case 0x47: createPort(); break;
		case 0x48: createSessionToPort(); break;
		case 0x49: createSession(); break;
		case 0x4A: acceptSession(); break;
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4E:
		case 0x4F:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
		case 0x66:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6E:
		case 0x6F:
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F:
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
		case 0x8C:
		case 0x8D:
		case 0x8E:
		case 0x8F:
		case 0x90:
			Helpers::warn("Reference SVC stub not yet implemented: %X @ %08X", svc, regs[15]);
			regs[0] = Result::OS::NotImplemented;
			break;

		// Luma SVCs
		case 0x91: regs[0] = Result::Success; break;  // FlushDataCacheRange (no-op for now)
		case 0x92: regs[0] = Result::Success; break;  // FlushEntireDataCache (no-op for now)
		case 0x93: svcInvalidateInstructionCacheRange(); break;
		case 0x94: svcInvalidateEntireInstructionCache(); break;
		case 0x95:
		case 0x96:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9A:
		case 0x9B:
		case 0x9C:
		case 0x9D:
		case 0x9E:
		case 0x9F:
		case 0xA0:
		case 0xA1:
		case 0xA2: controlMemory(); break;  // ControlMemoryEx
		case 0xA3: controlMemory(); break;  // ControlMemoryUnsafe
		case 0xA4:
		case 0xA5:
		case 0xA6:
		case 0xA7:
		case 0xA8:
		case 0xA9:
		case 0xAA:
		case 0xAB:
		case 0xAC:
		case 0xAD:
		case 0xAE:
		case 0xAF:
		case 0xB0:
		case 0xB1:
		case 0xB2:
		case 0xB3:
			Helpers::warn("Reference SVC stub not yet implemented: %X @ %08X", svc, regs[15]);
			regs[0] = Result::OS::NotImplemented;
			break;
		default:
			Helpers::warn("Unimplemented svc: %X @ %08X", svc, regs[15]);
			regs[0] = Result::OS::NotImplemented;
			break;
	}

	evalReschedule();
}


void Kernel::setMainThreadEntrypointAndSP(u32 entrypoint, u32 initialSP) {
	auto& t = threads[0];
	const bool isThumb = (entrypoint & 1) != 0;
	const u32 canonicalEntrypoint = entrypoint & ~1u;

	t.entrypoint = canonicalEntrypoint;
	t.initialSP = initialSP;
	t.gprs[13] = initialSP;
	t.gprs[15] = canonicalEntrypoint;
	t.cpsr = CPSR::UserMode | (isThumb ? CPSR::Thumb : 0);
	if (currentThreadIndex == 0) {
		cpu.setReg(13, initialSP);
		cpu.setReg(15, canonicalEntrypoint);
		cpu.setCPSR(t.cpsr);
	}
}

void Kernel::setVersion(u8 major, u8 minor) {
	u16 descriptor = (u16(major) << 8) | u16(minor);

	kernelVersion = descriptor;
	mem.kernelVersion = descriptor;  // The memory objects needs a copy because you can read the kernel ver from config mem
}

HorizonHandle Kernel::makeProcess(u32 id) {
	const Handle processHandle = makeObject(KernelObjectType::Process);
	const Handle resourceLimitHandle = makeObject(KernelObjectType::ResourceLimit);

	// Allocate data
	objects[processHandle].data = new Process(id);
	const auto processData = objects[processHandle].getData<Process>();
	processData->vmManager = mem.ensureProcessVM(processHandle);

	// Link resource limit object with its parent process
	objects[resourceLimitHandle].data = &processData->limits;
	processData->limits.handle = resourceLimitHandle;
	return processHandle;
}

void Kernel::reportMMUFault(u32 fsr, u32 far, bool instruction_fault, u32 abort_return_adjust) {
	auto* current = getObject(currentProcess, KernelObjectType::Process);
	if (current == nullptr) {
		return;
	}

	auto* process = current->getData<Process>();
	if (instruction_fault) {
		process->instrFaultStatus = fsr;
		process->instrFaultAddress = far;
	} else {
		process->dataFaultStatus = fsr;
		process->dataFaultAddress = far;
	}
	process->abortReturnAdjust = abort_return_adjust;
}

// Get a pointer to the process indicated by handle, taking into account that 0xFFFF8001 always refers to the current process
// Returns nullptr if the handle does not correspond to a process
KernelObject* Kernel::getProcessFromPID(Handle handle) {
	if (handle == KernelHandles::CurrentProcess) [[likely]] {
		return getObject(currentProcess, KernelObjectType::Process);
	} else {
		return getObject(handle, KernelObjectType::Process);
	}
}

void Kernel::deleteObjectData(KernelObject& object) {
	if (object.data == nullptr || !object.ownsData) {
		return;
	}

	// Resource limit and thread objects do not allocate heap data, so we don't delete anything

	switch (object.type) {
		case KernelObjectType::AddressArbiter: delete object.getData<AddressArbiter>(); return;
		case KernelObjectType::Archive: delete object.getData<ArchiveSession>(); return;
		case KernelObjectType::Directory: delete object.getData<DirectorySession>(); return;
		case KernelObjectType::Event: delete object.getData<Event>(); return;
		case KernelObjectType::File: delete object.getData<FileSession>(); return;
		case KernelObjectType::MemoryBlock: delete object.getData<MemoryBlock>(); return;
		case KernelObjectType::Port: delete object.getData<Port>(); return;
		case KernelObjectType::Process: delete object.getData<Process>(); return;
		case KernelObjectType::ResourceLimit: return;
		case KernelObjectType::Session: delete object.getData<Session>(); return;
		case KernelObjectType::Mutex: delete object.getData<Mutex>(); return;
		case KernelObjectType::Semaphore: delete object.getData<Semaphore>(); return;
		case KernelObjectType::Timer: delete object.getData<Timer>(); return;
		case KernelObjectType::Thread: return;
		case KernelObjectType::Dummy: return;
		default: [[unlikely]] Helpers::warn("unknown object type"); return;
	}
}

void Kernel::reset() {
	handleCounter = 0;
	arbiterCount = 0;
	threadCount = 0;
	aliveThreadCount = 0;

	for (auto& t : threads) {
		t.status = ThreadStatus::Dead;
		t.waitList.clear();
		t.threadsWaitingForTermination = 0;  // No threads are waiting for this thread to terminate cause it's dead
	}

	for (auto& object : objects) {
		deleteObjectData(object);
	}
	objects.clear();
	mutexHandles.clear();
	timerHandles.clear();
	portHandles.clear();
	threadIndices.clear();
	serviceManager.reset();

	nextScheduledWakeupTick = std::numeric_limits<u64>::max();
	needReschedule = false;

	// Allocate handle #0 to a dummy object and make a main process object
	makeObject(KernelObjectType::Dummy);
	currentProcess = makeProcess(1);  // Use ID = 1 for main process
	mem.activateProcessVM(currentProcess);

	// Make main thread object. We do not have to set the entrypoint and SP for it as the ROM loader does.
	// Main thread seems to have a priority of 0x30. TODO: This creates a dummy context for thread 0,
	// which is thankfully not used. Maybe we should prevent this
	mainThread = makeThread(0, VirtualAddrs::StackTop, 0x30, ProcessorID::Default, 0, ThreadStatus::Running);
	currentThreadIndex = 0;
	setupIdleThread();

	// Create some of the OS ports
	srvHandle = makePort("srv:");         // Service manager port
	errorPortHandle = makePort("err:f");  // Error display port
}

// Get pointer to thread-local storage
u32 Kernel::getTLSPointer() { return VirtualAddrs::TLSBase + currentThreadIndex * VirtualAddrs::TLSSize; }

// Result CloseHandle(Handle handle)
void Kernel::svcCloseHandle() {
	logSVC("CloseHandle(handle = %d) (Unimplemented)\n", regs[0]);
	const Handle handle = regs[0];

	KernelObject* object = getObject(handle);
	if (object != nullptr) {
		switch (object->type) {
			// Close file descriptor when closing a file to prevent leaks and properly flush file contents
			case KernelObjectType::File: {
				FileSession* file = object->getData<FileSession>();
				if (file->isOpen) {
					file->isOpen = false;

					if (file->fd != nullptr) {
						fclose(file->fd);
						file->fd = nullptr;
					}
				}
				break;
			}

			default: break;
		}
	}

	// Stub to always succeed for now
	regs[0] = Result::Success;
}

// u64 GetSystemTick()
void Kernel::getSystemTick() {
	logSVC("GetSystemTick()\n");

	u64 ticks = cpu.getTicks();
	regs[0] = u32(ticks);
	regs[1] = u32(ticks >> 32);
}

// Result OutputDebugString(const char* str, s32 size)
// TODO: Does this actually write an error code in r0 and is the above signature correct?
void Kernel::outputDebugString() {
	const u32 pointer = regs[0];
	const u32 size = regs[1];

	std::string message = mem.readString(pointer, size);
	logDebugString("[OutputDebugString] %s\n", message.c_str());
	regs[0] = Result::Success;
}

void Kernel::getProcessID() {
	const auto pid = regs[1];
	const auto process = getProcessFromPID(pid);
	logSVC("GetProcessID(process: %s)\n", getProcessName(pid).c_str());

	if (process == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	regs[0] = Result::Success;
	regs[1] = process->getData<Process>()->id;
}

// Result OpenProcess(Handle* out, ProcessId process_id)
void Kernel::openProcess() {
	const u32 pid = regs[1];
	const auto process = getProcessFromPID(pid);
	logSVC("OpenProcess(pid = %u)\n", pid);

	if (process == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::NotFound;
		return;
	}

	regs[1] = process->handle;
	duplicateHandle();
}

// Result OpenThread(Handle* out, ThreadId thread_id)
void Kernel::openThread() {
	const u32 threadID = regs[1];
	logSVC("OpenThread(thread ID = %u)\n", threadID);

	if (threadID >= threads.size()) [[unlikely]] {
		regs[0] = Result::Kernel::NotFound;
		return;
	}

	Thread& thread = threads[threadID];
	if (thread.status == ThreadStatus::Dead) [[unlikely]] {
		regs[0] = Result::Kernel::NotFound;
		return;
	}

	regs[1] = thread.handle;
	duplicateHandle();
}

// Result GetProcessIdOfThread(ProcessId* out, Handle thread)
void Kernel::getProcessIDOfThread() {
	const Handle handle = regs[1];
	logSVC("GetProcessIdOfThread(thread = %X)\n", handle);

	KernelObject* thread = nullptr;
	if (handle == KernelHandles::CurrentThread) {
		thread = getObject(threads[currentThreadIndex].handle, KernelObjectType::Thread);
	} else {
		thread = getObject(handle, KernelObjectType::Thread);
	}

	if (thread == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	const Handle owner = thread->getData<Thread>()->ownerProcess;
	const auto process = getObject(owner, KernelObjectType::Process);
	if (process == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::NotFound;
		return;
	}

	regs[0] = Result::Success;
	regs[1] = process->getData<Process>()->id;
}

// Result GetHandleInfo(s64* out, Handle handle, HandleInfoType type)
void Kernel::getHandleInfo() {
	const Handle handle = regs[1];
	const u32 type = regs[2];
	logSVC("GetHandleInfo(handle = %X, type = %u)\n", handle, type);

	const auto object = getObject(handle);
	if (object == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	// 3DBrew/Reference semantics:
	// 0 = process ticks (only meaningful on process handle)
	// 1 = reference count
	// 2,3 = stubbed values
	u64 value = 0;
	switch (type) {
		case 0:  // KPROCESS_ELAPSED_TICKS (stubbed)
			value = 0;
			break;
		case 1:  // REFERENCE_COUNT (approximation: 1 without full KObject ref tracker)
			value = 1;
			break;
		case 2:
		case 3:
			value = 0;
			break;
		default:
			regs[0] = Result::Kernel::InvalidEnumValue;
			return;
	}

	regs[0] = Result::Success;
	regs[1] = static_cast<u32>(value);
	regs[2] = static_cast<u32>(value >> 32);
}

// Result GetThreadInfo(s64* out, Handle thread, ThreadInfoType type)
void Kernel::getThreadInfo() {
	const Handle handle = regs[1];
	const u32 type = regs[2];
	logSVC("GetThreadInfo(thread = %X, type = %u)\n", handle, type);

	KernelObject* threadObject = nullptr;
	if (handle == KernelHandles::CurrentThread) {
		threadObject = getObject(threads[currentThreadIndex].handle, KernelObjectType::Thread);
	} else {
		threadObject = getObject(handle, KernelObjectType::Thread);
	}

	if (threadObject == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	const Thread* thread = threadObject->getData<Thread>();
	u64 value = 0;
	switch (type) {
		case 0:  // Priority
			value = thread->priority;
			break;
		case 1:  // Ideal processor
			value = static_cast<u32>(thread->processorID);
			break;
		default:
			regs[0] = Result::Kernel::InvalidEnumValue;
			return;
	}

	regs[0] = Result::Success;
	regs[1] = static_cast<u32>(value);
	regs[2] = static_cast<u32>(value >> 32);
}

// Result GetProcessInfo(s64* out, Handle process, ProcessInfoType type)
void Kernel::getProcessInfo() {
	const auto pid = regs[1];
	const auto type = regs[2];
	const auto process = getProcessFromPID(pid);
	logSVC("GetProcessInfo(process: %s, type = %d)\n", getProcessName(pid).c_str(), type);

	if (process == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	switch (type) {
		// Returns the amount of <related unused field> + total supervisor-mode stack size + page-rounded size of the external handle table
		case 1:
			Helpers::warn("GetProcessInfo: Unimplemented type 1");
			regs[1] = 0;
			regs[2] = 0;
			break;

		// According to 3DBrew: Amount of private (code, data, heap) memory used by the process + total supervisor-mode
		// stack size + page-rounded size of the external handle table
		case 2:
			regs[1] = fcramManager.getUsedCount(FcramRegion::App) * Memory::pageSize;
			regs[2] = 0;
			break;

		case 20:  // Returns 0x20000000 - <linear memory base vaddr for process>
			regs[1] = PhysicalAddrs::FCRAM - mem.getLinearHeapVaddr();
			regs[2] = 0;
			break;

		default:
			Helpers::warn("GetProcessInfo: unknown type %d\n", type);
			regs[0] = Result::Kernel::InvalidEnumValue;
			return;
	}

	regs[0] = Result::Success;
}

// Result DuplicateHandle(Handle* out, Handle original)
void Kernel::duplicateHandle() {
	Handle original = regs[1];
	logSVC("DuplicateHandle(handle = %X)\n", original);

	if (original == KernelHandles::CurrentThread) {
		original = threads[currentThreadIndex].handle;
	} else if (original == KernelHandles::CurrentProcess) {
		original = currentProcess;
	}

	KernelObject* object = getObject(original);
	if (object == nullptr) [[unlikely]] {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	const Handle duplicate = makeObject(object->type);
	objects[duplicate].data = object->data;
	objects[duplicate].ownsData = false;

	regs[0] = Result::Success;
	regs[1] = duplicate;
}

// Result SignalAndWait(Handle signal, Handle wait, s64 timeout_ns)
void Kernel::signalAndWait() {
	const Handle signalHandle = regs[0];
	const Handle waitHandle = regs[1];
	const s64 timeoutNs = s64(u64(regs[2]) | (u64(regs[3]) << 32));
	logSVC("SignalAndWait(signal = %X, wait = %X, timeout = %lld)\n", signalHandle, waitHandle, timeoutNs);

	if (getObject(signalHandle, KernelObjectType::Event) != nullptr) {
		signalEvent(signalHandle);
	} else if (getObject(signalHandle, KernelObjectType::Semaphore) != nullptr) {
		if (!releaseSemaphore(signalHandle, 1, nullptr)) [[unlikely]] {
			regs[0] = Result::Kernel::InvalidHandle;
			return;
		}
	} else {
		regs[0] = Result::Kernel::InvalidHandle;
		return;
	}

	regs[0] = waitHandle;
	waitSynchronization1();
}

void Kernel::clearInstructionCache() { cpu.clearCache(); }
void Kernel::clearInstructionCacheRange(u32 start, u32 size) { cpu.clearCacheRange(start, size); }

void Kernel::svcInvalidateInstructionCacheRange() {
	const u32 start = regs[0];
	const u32 size = regs[1];
	logSVC("svcInvalidateInstructionCacheRange(start = %08X, size = %08X)\n", start, size);

	clearInstructionCacheRange(start, size);
	regs[0] = Result::Success;
}

void Kernel::svcInvalidateEntireInstructionCache() {
	logSVC("svcInvalidateEntireInstructionCache()\n");

	clearInstructionCache();
	regs[0] = Result::Success;
}

namespace SystemInfoType {
	enum : u32 {
		MemoryInformation = 0,
		// Gets information related to Citra (We don't implement this, we just report this emulator is not Citra)
		CitraInformation = 0x20000,
		// Gets information related to this emulator
		PandaInformation = 0x20001,
	};
};

namespace CitraInfoType {
	enum : u32 {
		IsCitra = 0,
		BuildName = 10,     // (ie: Nightly, Canary).
		BuildVersion = 11,  // Build version.
		BuildDate1 = 20,    // Build date first 7 characters.
		BuildDate2 = 21,    // Build date next 7 characters.
		BuildDate3 = 22,    // Build date next 7 characters.
		BuildDate4 = 23,    // Build date last 7 characters.
		BuildBranch1 = 30,  // Git branch first 7 characters.
		BuildBranch2 = 31,  // Git branch last 7 characters.
		BuildDesc1 = 40,    // Git description (commit) first 7 characters.
		BuildDesc2 = 41,    // Git description (commit) last 7 characters.
	};
}

namespace PandaInfoType {
	enum : u32 {
		IsPanda = 0,
	};
}

void Kernel::getSystemInfo() {
	const u32 infoType = regs[1];
	const u32 subtype = regs[2];
	log("GetSystemInfo (type = %X, subtype = %X)\n", infoType, subtype);

	regs[0] = Result::Success;
	switch (infoType) {
		case SystemInfoType::MemoryInformation: {
			switch (subtype) {
				// Total used memory size in the APPLICATION memory region
				case 1:
					regs[1] = fcramManager.getUsedCount(FcramRegion::App) * Memory::pageSize;
					regs[2] = 0;
					break;

				default:
					Helpers::panic("GetSystemInfo: Unknown MemoryInformation subtype %x\n", subtype);
					regs[0] = Result::FailurePlaceholder;
					break;
			}
			break;
		}

		case SystemInfoType::CitraInformation: {
			switch (subtype) {
				case CitraInfoType::IsCitra:
					// Report that we're not Citra
					regs[1] = 0;
					regs[2] = 0;
					break;

				default:
					Helpers::warn("GetSystemInfo: Unknown CitraInformation subtype %x\n", subtype);
					regs[0] = Result::FailurePlaceholder;
					break;
			}

			break;
		}

		case SystemInfoType::PandaInformation: {
			switch (subtype) {
				case PandaInfoType::IsPanda:
					// This is indeed us, set output to 1
					regs[1] = 1;
					regs[2] = 0;
					break;

				default:
					Helpers::warn("GetSystemInfo: Unknown PandaInformation subtype %x\n", subtype);
					regs[0] = Result::FailurePlaceholder;
					break;
			}

			break;
		}

		default: Helpers::panic("GetSystemInfo: Unknown system info type: %x (subtype: %x)\n", infoType, subtype); break;
	}
}

std::string Kernel::getProcessName(u32 pid) {
	if (pid == KernelHandles::CurrentProcess) {
		return "current";
	}

	const auto* process = getObject(pid, KernelObjectType::Process);
	if (process != nullptr) {
		return std::to_string(process->getData<Process>()->id);
	}

	return std::to_string(pid);
}

Scheduler& Kernel::getScheduler() { return cpu.getScheduler(); }
