#!/usr/bin/env python3
#
# About:
#   Prelink script for Hitagi RAMDLD project.
#   It generates proper linker script from template.
#
# Author:
#   EXL, ChatGPT-4.1 (GitHub Copilot)
#
# License:
#   MIT
#

import sys

def main():
	if len(sys.argv) not in (5, 6):
		print('Usage: python prelink.py <template.lds> <output.ld> <ORIGIN> <LENGTH> [OUTPUT_FORMAT]')
		sys.exit(1)

	in_file  = sys.argv[1]
	out_file = sys.argv[2]
	origin   = sys.argv[3]
	length   = sys.argv[4]
	output_format = sys.argv[5] if len(sys.argv) == 6 else 'elf32-bigarm'

	with open(in_file, 'r', encoding='utf-8') as f:
		content = f.read()

	content = content.replace('%ORIGIN%', origin)
	content = content.replace('%LENGTH%', length)
	content = content.replace('%OUTPUT_FORMAT%', output_format)

	with open(out_file, 'w', encoding='utf-8') as f:
		f.write(content)

	print(f'Patched {in_file} -> {out_file} with ORIGIN={origin}, LENGTH={length}, OUTPUT_FORMAT={output_format}')

if __name__ == '__main__':
	main()
