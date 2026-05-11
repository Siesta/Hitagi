#!/usr/bin/env python3
#
# About:
#   Postlink script for Hitagi RAMDLD project.
#   It inserts some additional payload data like proper header and sign.
#
# Author:
#   EXL, ChatGPT-4.1 (GitHub Copilot)
#
# License:
#   MIT
#

import sys
import os

def read_be32(data, offset):
	return int.from_bytes(data[offset:offset + 4], 'big')

def main(head_path, loader_path, output_path, sign_path=None, sign_offset=None):
	# Step 1: Read input files.
	with open(head_path, 'rb') as f:
		head_data = f.read()
	with open(loader_path, 'rb') as f:
		loader_data = f.read()

	# Step 2: Concatenate 'head.bin' and 'loader.bin' files.
	combined = head_data + loader_data

	if sign_path is None:
		if len(head_data) >= 0x14:
			start_addr = read_be32(head_data, 0x00)
			end_addr = read_be32(head_data, 0x10)
			min_size = len(head_data) + (end_addr - start_addr)
			if end_addr > start_addr and len(combined) < min_size:
				combined += b'\xFF' * (min_size - len(combined))
		with open(output_path, 'wb') as f:
			f.write(combined)
		return

	with open(sign_path, 'rb') as f:
		sign_data = f.read()

	# Step 3: Prepare output buffer filled with 0xFFs up to offset.
	if len(combined) > sign_offset:
		raise ValueError(f'Combined size (0x{len(combined):08X}) is greater than sign offset (0x{sign_offset:08X})')
	# Pad with 0xFF up to offset.
	out_buf = bytearray(combined)
	out_buf.extend(b'\xFF' * (sign_offset - len(out_buf)))

	# Step 4: Insert 'sign.bin' at offset.
	out_buf.extend(b'\xFF' * (sign_offset + len(sign_data) - len(out_buf))) # ensure enough space
	out_buf[sign_offset:sign_offset+len(sign_data)] = sign_data

	# Step 5: Write to output file.
	with open(output_path, 'wb') as f:
		f.write(out_buf)

if __name__ == '__main__':
	if len(sys.argv) not in (4, 6):
		print('Usage: python postlink.py <head.bin> <loader.bin> <output.bin>')
		print('   or: python postlink.py <head.bin> <loader.bin> <sign.bin> <SIGN_OFFSET> <loader.ldr>')
		sys.exit(1)
	head_path = sys.argv[1]
	loader_path = sys.argv[2]
	if len(sys.argv) == 4:
		output_path = sys.argv[3]
		main(head_path, loader_path, output_path)
	else:
		sign_path = sys.argv[3]
		sign_offset = int(sys.argv[4], 16)
		output_path = sys.argv[5]
		main(head_path, loader_path, output_path, sign_path, sign_offset)
