import os

env = Environment(
	ENV = os.environ,
)

SConscript('laks/build_rules')

env.SelectMCU('stm32f303rc')

arc = ARGUMENTS.get('arcin',0)
if int(arc):
	env.Append(CPPDEFINES = ['ARCIN'])
	filename = 'arcin-roxy.elf'
else:
	env.Append(CPPDEFINES = ['ROXY'])
	filename = 'roxy.elf'

env.Firmware(filename, Glob('roxy/*.cpp') + Glob('roxy/rgb/*.cpp'), LINK_SCRIPT = 'roxy/roxy.ld')

env.Firmware('bootloader.elf', Glob('bootloader/*.cpp'), LINK_SCRIPT = 'bootloader/bootloader.ld')

env.Firmware('test.elf', Glob('test/*.cpp'))
