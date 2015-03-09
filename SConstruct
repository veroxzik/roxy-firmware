import os

env = Environment(
	ENV = os.environ,
)

SConscript('laks/build_rules')

env.SelectMCU('stm32f103vb')

env.Firmware('arcin.elf', ['main.cpp'])
