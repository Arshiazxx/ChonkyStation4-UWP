#!/usr/bin/env python3
from pathlib import Path
import json
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
MEM = ROOT / 'platform' / 'xbox' / 'Memory'
PROBE = ROOT / 'platform' / 'xbox' / 'XboxCapabilityProbe'
UPSTREAM_MAIN = ROOT / 'ChonkyStation4' / 'ChonkyStation4.cpp'
UPSTREAM_KERNEL = ROOT / 'ChonkyStation4' / 'OS' / 'Libraries' / 'Kernel' / 'Kernel.cpp'
UPSTREAM_ELF = ROOT / 'ChonkyStation4' / 'Loaders' / 'ELF' / 'ELFLoader.cpp'
UPSTREAM_PATCHER = ROOT / 'ChonkyStation4' / 'Loaders' / 'ELF' / 'CodePatcher.cpp'
BASELINE = ROOT / 'xbox-guest-memory.json'

required = [
    MEM / 'XboxGuestMemory.hpp', MEM / 'XboxGuestMemory.cpp',
    PROBE / 'XboxCapabilityProbe.vcxproj', PROBE / 'Package.appxmanifest',
    BASELINE,
]
for path in required:
    if not path.exists():
        print(f'ERROR missing {path}', file=sys.stderr)
        raise SystemExit(1)

ET.parse(PROBE / 'XboxCapabilityProbe.vcxproj')
ET.parse(PROBE / 'Package.appxmanifest')

header = (MEM / 'XboxGuestMemory.hpp').read_text()
source = (MEM / 'XboxGuestMemory.cpp').read_text()
project = (PROBE / 'XboxCapabilityProbe.vcxproj').read_text()
manifest = (PROBE / 'Package.appxmanifest').read_text()
main = UPSTREAM_MAIN.read_text(encoding='utf-8-sig')
kernel = UPSTREAM_KERNEL.read_text(encoding='utf-8-sig')
elf = UPSTREAM_ELF.read_text(encoding='utf-8-sig')
patcher = UPSTREAM_PATCHER.read_text(encoding='utf-8-sig')

# Ensure the diagnostic mirrors current upstream constants rather than inventing a layout.
for token in ['0x0\'8000\'0000', '2048_GB']:
    if token not in main:
        raise SystemExit(f'ERROR upstream reservation token changed/missing: {token}')
for token in ["0x8000'0000", '2000_GB', "0x0010'0000'0000", '16_KB']:
    if token not in kernel:
        raise SystemExit(f'ERROR upstream kernel memory token changed/missing: {token}')
for token in ['ReservationBase = 0x0000000080000000ull', 'ReservationSize = 2048ull', 'AllocationSearchSize = 2000ull', 'SystemMappingArea = 0x0000001000000000ull', 'GuestPageSize = 16ull * 1024ull']:
    if token not in header:
        raise SystemExit(f'ERROR M6 layout token missing: {token}')

for token in ['VirtualAllocFromApp', 'VirtualProtectFromApp', 'VirtualQuery', 'VirtualFree', 'MEM_RESERVE', 'MEM_COMMIT', 'MEM_DECOMMIT', 'CreateFileMappingFromApp', 'MapViewOfFileFromApp']:
    if token not in source:
        raise SystemExit(f'ERROR M6 implementation missing {token}')

if 'PAGE_EXECUTE' in source:
    raise SystemExit('ERROR M6 guest-memory foundation must not implement executable pages; M7 owns executable/native execution')
if 'XboxGuestMemory.cpp' not in project:
    raise SystemExit('ERROR XboxCapabilityProbe does not compile the M6 diagnostic source')
if '<ClCompile Include="..\\Memory\\XboxGuestMemory.cpp"><PrecompiledHeader>NotUsing</PrecompiledHeader></ClCompile>' not in project:
    raise SystemExit('ERROR M6 standalone memory source must disable the probe PCH explicitly')
if 'codeGeneration' not in manifest:
    raise SystemExit('ERROR probe manifest lacks codeGeneration capability required by FromApp protection/mapping diagnostics')

# Preserve the M7 risk in validation: current upstream still requests RWX in both loader and patcher.
if 'PAGE_EXECUTE_READWRITE' not in elf or 'PAGE_EXECUTE_READWRITE' not in patcher:
    raise SystemExit('ERROR expected upstream RWX M7 risk is no longer present; re-audit M6/M7 assumptions')

data = json.loads(BASELINE.read_text())
if data.get('run_state') != 'NOT_RUN':
    raise SystemExit('ERROR xbox-guest-memory.json baseline claims a runtime test occurred')
if data.get('source_commit') != '310269290a3c256f5911d4bc7e441489bffffbf6':
    raise SystemExit('ERROR xbox-guest-memory.json source commit mismatch')
if data.get('guest_base') != '0x0000000080000000' or data.get('reservation_bytes') != 2199023255552:
    raise SystemExit('ERROR xbox-guest-memory.json upstream layout mismatch')

print('PASS: M6 layout matches current upstream constants')
print('PASS: UWP data-memory reservation/commit/protect/decommit/shared-map diagnostics are present')
print('PASS: M6 contains no executable-page or guest-execution implementation')
print('PASS: baseline xbox-guest-memory.json is NOT_RUN')
