/**
reference https://gist.github.com/iMoD1998/4aa48d5c990535767a3fc3251efc0348  (10x cleaner version than legacy detour class)
reference https://github.com/skiff/libpsutil/blob/master/libpsutil/system/memory.cpp   (tocOverride idea)
reference https://pastebin.com/yezsesij   (legacy detour class)
Check https://pastebin.com/VCfMb46E  (for GetCurrentToc(), WriteProcessMemory, MARK_AS_EXECUTABLE)

TODO:
- make derive class for imports and exports
- make derive class for class symbol hooking. e.g: "17CNetworkObjectMgr" on gtav
- make derive class for hooking a single instruction b or bl
- make derive class for hooking a vtable functions or integrate with "class symbol hooking"
- ???? HookByFnid @ImportExportHook "not implemented" seriously whats the point of the derived class. <- that needs to be changed

*/
#ifndef BITCH
#define BITCH 1

#include <ppu_asm_intrinsics.h> // __ALWAYS_INLINE
#include <ppu_intrinsics.h>
#include <sys/process.h> // sys_process_getpid


#define MARK_AS_EXECUTABLE __attribute__((section(".text")))
#define GetRawDestination(type, destination) (type)*(type*)(destination)
#define PROC_TOC              0x01C85330

struct opd_s
{
	uint32_t sub;
	uint32_t toc;
};

static bool use_hen_syscalls = false;

static uint32_t sys_dbg_write_process_memory(uint32_t pid, void* address, const void* data, size_t size)
{
	system_call_4(905, (uint64_t)pid, (uint64_t)address, (uint64_t)size, (uint64_t)data);
	return_to_user_prog(uint32_t);
}

static uint32_t sys_hen_write_process_memory(uint32_t pid, void* address, const void* data, size_t size)
{
	system_call_6(8, 0x7777, 0x32, (uint64_t)pid, (uint64_t)address, (uint64_t)data, (uint64_t)size);
	return_to_user_prog(uint32_t);
}

static uint32_t WriteProcessMemory(uint32_t pid, void* address, const void* data, size_t size)
{
	if (!use_hen_syscalls)
	{
		uint32_t write = sys_dbg_write_process_memory(pid, address, data, size);
		if (write == SUCCEEDED)
		{
			return write;
		}
	}

	use_hen_syscalls = true;
	return sys_hen_write_process_memory(pid, address, data, size);
}

static uint32_t GetCurrentToc()
{
	uint32_t* entry_point = *reinterpret_cast<uint32_t**>(0x1001C); // ElfHeader->e_entry 
	return entry_point[1];
}

template <typename R>
R(*GameCall(std::uint32_t addr, std::uint32_t toc = PROC_TOC))(...)
{
	volatile opd_s opd = { addr, toc };
	R(*func)(...) = (R(*)(...))&opd;
	return func;
}

// when calling some game functions with GameCallV2 the game crashes in Minecraft PS3 Edition so use GameCall instead
template <typename R, typename... TArgs>
R GameCallV2(std::uint32_t addr, TArgs... args)
{
	volatile opd_s opd = { addr, PROC_TOC };
	R(*func)(TArgs...) = (R(*)(TArgs...))&opd;
	return func(args...);
}

template <typename R>
R(*VmtCall(std::uint32_t vmt, std::uint32_t offset))(...)
{
	std::uint32_t* funcAdr = (std::uint32_t*)(*(volatile std::uint32_t*)(vmt + offset));
	volatile opd_s opd = { funcAdr[0], funcAdr[1] };
	R(*func)(...) = (R(*)(...))&opd;
	return func;
}

template <typename var>
bool write_mem(uint32_t address, var value)
{
	return (WriteProcessMemory(sys_process_getpid(), (void*)address, &value, sizeof(var)) == SUCCEEDED);
}


static __attribute__((naked)) void _savegpr0_N()
{
	// https://pastebin.com/a7Ci8aAi
	__asm
	(
		"std  %r14, -144(%r1);"         // _savegpr0_14
		"std  %r15, -136(%r1);"         // _savegpr0_15
		"std  %r16, -128(%r1);"         // _savegpr0_16
		"std  %r17, -120(%r1);"         // _savegpr0_17
		"std  %r18, -112(%r1);"         // _savegpr0_18
		"std  %r19, -104(%r1);"         // _savegpr0_19
		"std  %r20, -96(%r1);"          // _savegpr0_20
		"std  %r21, -88(%r1);"          // _savegpr0_21
		"std  %r22, -80(%r1);"          // _savegpr0_22
		"std  %r23, -72(%r1);"          // _savegpr0_23
		"std  %r24, -64(%r1);"          // _savegpr0_24
		"std  %r25, -56(%r1);"          // _savegpr0_25
		"std  %r26, -48(%r1);"          // _savegpr0_26
		"std  %r27, -40(%r1);"          // _savegpr0_27
		"std  %r28, -32(%r1);"          // _savegpr0_28
		"std  %r29, -24(%r1);"          // _savegpr0_29
		"std  %r30, -16(%r1);"          // _savegpr0_30
		"std  %r31, -8(%r1);"           // _savegpr0_31
		"std  %r0, 16(%r1);"
		"blr;"
		);
}

static uint32_t RelinkGPLR(uint32_t SFSOffset, uint32_t* SaveStubAddress, uint32_t* OriginalAddress)
{
	uint32_t Instruction = 0, Replacing;
	uint32_t* Saver = *(uint32_t**)_savegpr0_N;

	if (SFSOffset & 0x2000000)
		SFSOffset |= 0xFC000000;

	Replacing = OriginalAddress[SFSOffset / 4];

	for (int i = 0; i < 20; i++)
	{
		if (Replacing == Saver[i])
		{
			int NewOffset = (int)&Saver[i] - (int)SaveStubAddress;
			Instruction = 0x48000001 | (NewOffset & 0x3FFFFFC);
		}
	}
	return Instruction;
}

static void PatchInJump(uint32_t* address, uint32_t destination, bool linked)
{
	uint32_t inst_lis = 0x3D600000 + ((destination >> 16) & 0xFFFF); // lis %r11, dest>>16
	write_mem<uint32_t>((uint32_t)&address[0], inst_lis);
	if (destination & 0x8000)   // if bit 16 is 1
		write_mem<uint32_t>((uint32_t)&address[0], inst_lis += 1);

	write_mem<uint32_t>((uint32_t)&address[1], 0x396B0000 + (destination & 0xFFFF)); // addi %r11, %r11, dest&OxFFFF
	write_mem<uint32_t>((uint32_t)&address[2], 0x7D6903A6); // mtctr %r11
	write_mem<uint32_t>((uint32_t)&address[3], 0x4E800420); // bctr
	if (linked)
		write_mem<uint32_t>((uint32_t)&address[3], 0x4E800421); // bctrl

	__dcbst(address);
	__sync();
	__isync();
}

static void HookFunctionStart(uint32_t* address, uint32_t destination, uint32_t* original)
{
	uint32_t addressRelocation = (uint32_t)(&address[4]); // Replacing 4 instructions with a jump, this is the stub return address

	// build the stub
	// make a jump to go to the original function start+4 instructions
	uint32_t inst_lis = 0x3D600000 + ((addressRelocation >> 16) & 0xFFFF); // lis r11, 0 | Load Immediate Shifted
	write_mem<uint32_t>((uint32_t)&original[0], inst_lis);

	if (addressRelocation & 0x8000) // If bit 16 is 1
		write_mem<uint32_t>((uint32_t)&original[0], inst_lis += 1); // lis r11, 0 | Load Immediate Shifted

	write_mem<uint32_t>((uint32_t)&original[1], 0x396B0000 + (addressRelocation & 0xFFFF)); // addi r11, r11, (value of AddressRelocation & 0xFFFF) | Add Immediate
	write_mem<uint32_t>((uint32_t)&original[2], 0x7D6903A6); // mtspr CTR, r11 | Move to Special-Purpose Register CTR 

	// Instructions [3] through [6] are replaced with the original instructions from the function hook
	// Copy original instructions over, relink stack frame saves to local ones
	for (int i = 0; i < 4; i++)
	{
		if ((address[i] & 0xF8000000) == 0x48000000) // branch with link
		{
			write_mem<uint32_t>((uint32_t)&original[i + 3], RelinkGPLR((address[i] & ~0x48000003), &original[i + 3], &address[i]));
		}
		else
		{
			write_mem<uint32_t>((uint32_t)&original[i + 3], address[i]);
		}
	}
	write_mem<uint32_t>((uint32_t)&original[7], 0x4E800420); // bctr | Branch unconditionally 

	__dcbst(original); // Data Cache Block Store | Allows a program to copy the contents of a modified block to main memory.
	__sync(); // Synchronize | Ensure the dcbst instruction has completed.
	__isync(); // Instruction Synchronize | Refetches any instructions that might have been fetched prior to this instruction.

	PatchInJump(address, *(uint32_t*)destination, false); // Redirect Function to ours

	/*
	* So in the end, this will produce:
	*
	* lis r11, ((AddressRelocation >> 16) & 0xFFFF [+ 1])
	* addi r11, r11, (AddressRelocation & 0xFFFF)
	* mtspr CTR, r11
	* branch (GPR)
	* dcbst (SaveStub)
	* sync
	*/
}

#define MAX_HOOKFUNCTIONS_ASM_SIZE 300
static MARK_AS_EXECUTABLE uint8_t g_hookFunctionsAsm[300];
static int g_hookFunctionsAsmCount = 0;

static bool HookFunctionStartAuto(uint32_t* address, uint32_t destination, uint32_t** original)
{
	// check for buffer overflow
	if (g_hookFunctionsAsmCount > (MAX_HOOKFUNCTIONS_ASM_SIZE - 2))
		return false;

	uint32_t* stubAddr = (uint32_t*)&g_hookFunctionsAsm[g_hookFunctionsAsmCount];

	HookFunctionStart(address, destination, stubAddr);

	// write an opd into memory
	write_mem<uint64_t>((uint32_t)&stubAddr[8], ((uint64_t)stubAddr << 32) | GetCurrentToc()); // (uint32_t)__builtin_get_toc()
	*original = &stubAddr[8];

	// Increment asm byte count
	// 0x20 bytes from HookFunctionStart
	// 0x8 bytes for opd
	g_hookFunctionsAsmCount += 0x28;

	return true;
}

static void PatchInBranch(uint32_t* address, uint32_t destination, bool linked)
{
	destination = GetRawDestination(uint32_t, destination);
	uint32_t inst_branch = 0x48000000 + ((destination - (uint32_t)address) & 0x3FFFFFF); // b Destination
	write_mem<uint32_t>((uint32_t)&address[0], inst_branch);

	if (linked)
		write_mem<uint32_t>((uint32_t)&address[0], inst_branch += 1); // bl Destination
}

static void PatchInVmt(uint32_t* address, uint32_t destination)
{
	write_mem<uint64_t>((uint32_t)&address[0], GetRawDestination(uint64_t, destination)); // uint64_t so it includes the toc
}



#include <string>

//struct opd_s
//{
//   uint32_t sub;
//   uint32_t toc;
//};

struct importStub_s
{
	int16_t ssize;
	int16_t header1;
	int16_t header2;
	int16_t imports;
	int32_t zero1;
	int32_t zero2;
	const char* name;
	uint32_t* fnid;
	opd_s** stub;
	int32_t zero3;
	int32_t zero4;
	int32_t zero5;
	int32_t zero6;
};

struct exportStub_s
{
	int16_t ssize;
	int16_t header1;
	int16_t header2;
	int16_t exports; // number of exports
	int32_t zero1;
	int32_t zero2;
	const char* name;
	uint32_t* fnid;
	opd_s** stub;
};

#define MARK_AS_EXECUTABLE __attribute__((section(".text")))

class DetourHook
{
public:
	DetourHook();
	DetourHook(uintptr_t fnAddress, uintptr_t fnCallback);
	DetourHook(DetourHook const&) = delete;
	DetourHook(DetourHook&&) = delete;
	DetourHook& operator=(DetourHook const&) = delete;
	DetourHook& operator=(DetourHook&&) = delete;
	virtual ~DetourHook();

	virtual void Hook(uintptr_t fnAddress, uintptr_t fnCallback, uintptr_t tocOverride = 0);
	virtual bool UnHook();

	// also works
	/*template<typename T>
	T GetOriginal() const
	{
	return T(m_TrampolineOpd);
	}*/

	template <typename R, typename... TArgs>
	R GetOriginal(TArgs... args)
	{
		R(*original)(TArgs...) = (R(*)(TArgs...))m_TrampolineOpd;
		return original(args...);
	}


private:
	/***
	* Writes an unconditional branch to the destination address that will branch to the target address.
	* @param destination Where the branch will be written to.
	* @param branchTarget The address the branch will jump to.
	* @param linked Branch is a call or a jump? aka bl or b
	* @param preserveRegister Preserve the register clobbered after loading the branch address.
	* @returns size of relocating the instruction in bytes
	*/
	size_t Jump(void* destination, const void* branchTarget, bool linked, bool preserveRegister);

	/***
	* Writes both conditional and unconditional branches using the count register to the destination address that will branch to the target address.
	* @param destination Where the branch will be written to.
	* @param branchTarget The address the branch will jump to.
	* @param linked Branch is a call or a jump? aka bl or b
	* @param preserveRegister Preserve the register clobbered after loading the branch address.
	* @param branchOptions Options for determining when a branch to be followed.
	* @param conditionRegisterBit The bit of the condition register to compare.
	* @param registerIndex Register to use when loading the destination address into the count register.
	* @returns size of relocating the instruction in bytes
	*/
	size_t JumpWithOptions(void* destination, const void* branchTarget, bool linked, bool preserveRegister,
		uint32_t branchOptions, uint8_t conditionRegisterBit, uint8_t registerIndex);

	/***
	* Copies and fixes relative branch instructions to a new location.
	* @param destination Where to write the new branch.
	* @param source Address to the instruction that is being relocated.
	* @returns size of relocating the instruction in bytes
	*/
	size_t RelocateBranch(uint32_t* destination, uint32_t* source);

	/***
	* Copies an instruction enusuring things such as PC relative offsets are fixed.
	* @param destination Where to write the new instruction(s).
	* @param source Address to the instruction that is being copied.
	* @returns size of relocating the instruction in bytes
	*/
	size_t RelocateCode(uint32_t* destination, uint32_t* source);

	/***
	* Get's size of method hook in bytes
	* @param branchTarget The address the branch will jump to.
	* @param linked Branch is a call or a jump? aka bl or b
	* @param preserveRegister Preserve the register clobbered after loading the branch address.
	* @returns size of hook in bytes
	*/
	size_t GetHookSize(const void* branchTarget, bool linked, bool preserveRegister);

protected:
	const void*  m_HookTarget;                // The funtion we are pointing the hook to.
	void*        m_HookAddress;               // The function we are hooking.
	uint8_t*     m_TrampolineAddress;         // Pointer to the trampoline for this detour.
	uint32_t     m_TrampolineOpd[2];          // opd_s of the trampoline for this detour.
	uint8_t      m_OriginalInstructions[30];  // Any bytes overwritten by the hook.
	size_t       m_OriginalLength;            // The amount of bytes overwritten by the hook.

	// Shared
	MARK_AS_EXECUTABLE static uint8_t   s_TrampolineBuffer[1024];
	static size_t                       s_TrampolineSize;
};

// list of fnids https://github.com/aerosoul94/ida_gel/blob/master/src/ps3/ps3.xml
class ImportExportHook : public DetourHook
{
public:
	enum HookType { Import = 0, Export = 1 };
public:
	ImportExportHook(HookType type, const std::string& libaryName, uint32_t fnid, uintptr_t fnCallback);
	virtual ~ImportExportHook();

	virtual void Hook(uintptr_t fnAddress, uintptr_t fnCallback, uintptr_t tocOverride = 0) override;
	virtual bool UnHook() override;

	static opd_s* FindExportByName(const char* module, uint32_t fnid);
	static opd_s* FindImportByName(const char* module, uint32_t fnid);

private:
	void HookByFnid(HookType type, const std::string& libaryName, uint32_t fnid, uintptr_t fnCallback);

private:
	std::string m_LibaryName;
	uint32_t m_Fnid;
};
#endif