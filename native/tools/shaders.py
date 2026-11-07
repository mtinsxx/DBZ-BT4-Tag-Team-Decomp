#!/usr/bin/env python3
"""Offline shader compilation only; no game data or on-device compiler."""
import pathlib,subprocess,sys
source,output=map(pathlib.Path,sys.argv[1:])
output.parent.mkdir(parents=True,exist_ok=True)
lines=['#pragma once\n#include <cstdint>\nnamespace tenkaichi {\n']
for suffix,name in [('vert','vertex'),('frag','fragment')]:
    binary=output.parent/(name+'.spv')
    subprocess.run(['glslangValidator','-V','--target-env','vulkan1.0',str(source/('resource.'+suffix)),'-o',str(binary)],check=True)
    subprocess.run(['spirv-val','--target-env','vulkan1.0',str(binary)],check=True)
    data=binary.read_bytes()
    lines.append('alignas(4) inline constexpr unsigned char '+name+'[]={'+','.join(map(str,data))+'};\n')
lines.append('}\n');output.write_text(''.join(lines))
