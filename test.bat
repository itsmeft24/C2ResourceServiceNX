make
del "D:\Emulators\ryujinx_main\user\mods\contents\01006a800016e000\romfs\skyline\plugins\libcpp_skyline_plugin.nro"
xcopy libcpp_skyline_plugin.nro "D:\Emulators\ryujinx_main\user\mods\contents\01006a800016e000\romfs\skyline\plugins" /y
start D:\Emulators\ryujinx_main\publish\Ryujinx.exe -r d:\Emulators\ryujinx_main\user "d:\switch\Super Smash Bros. Ultimate.nsp"