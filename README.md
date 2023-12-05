# PS3Plugins
Compile with 
* General->Platform Toolset: `PPU SNC`
* Linker->General->Additional Dependencies: `libsn.a;libm.a;libio_stub.a;libfs_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libdbg_libio_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libc_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libm.a;$(SCE_PS3_ROOT)\target\ppu\lib\libsysutil_np_trophy_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libio_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libsysutil_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libsysmodule_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libsyscall.a;$(SCE_PS3_ROOT)\target\ppu\lib\libgcm_sys_stub.a;$(SCE_PS3_ROOT)\host-win32\spu\lib\gcc\spu-lv2\4.1.1\libgcc.a;$(SCE_PS3_ROOT)\target\ppu\lib\libpadfilter.a;$(SCE_PS3_ROOT)\target\ppu\lib\libstdc++.a;C:\cell\host-win32\ppu\lib\gcc\ppu-lv2\4.1.1\libsupc++.a;$(SCE_PS3_ROOT)\target\ppu\lib\libsysutil_oskdialog_ext_stub.a;$(SCE_PS3_ROOT)\target\ppu\lib\libnet_stub.a`
* C/C++->Language->C++ Language Standard: `Use C++11 (-Xstd=cpp11)`
