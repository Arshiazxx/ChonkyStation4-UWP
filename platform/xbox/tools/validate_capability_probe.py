#!/usr/bin/env python3
from pathlib import Path
import json, sys, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
PROBE=ROOT/'xbox'/'XboxCapabilityProbe'
proj=PROBE/'XboxCapabilityProbe.vcxproj'
manifest=PROBE/'Package.appxmanifest'
source=PROBE/'Probe'/'CapabilityProbe.cpp'
baseline=ROOT.parents[0]/'xbox-capabilities.json'
for f in [proj,manifest,source,PROBE/'App.xaml',PROBE/'MainPage.xaml']:
    if not f.exists(): print('ERROR missing',f,file=sys.stderr); raise SystemExit(1)
ET.parse(proj); ET.parse(manifest)
text=source.read_text(encoding="utf-8")

manifest_text=manifest.read_text(encoding="utf-8")
if 'codeGeneration' not in manifest_text:
    raise SystemExit('ERROR capability probe must declare codeGeneration for UWP executable-page transition probe')
if 'PAGE_EXECUTE_READWRITE' in text:
    raise SystemExit('ERROR M5 UWP probe must not rely on RWX pages; use W->RX and leave execution to M7')
for token in ['VirtualAllocFromApp','VirtualProtectFromApp','FlushInstructionCache','std::thread','CreateDXGIFactory2','D3D12CreateDevice','CheckFeatureSupport','CreateDescriptorHeap','CreateCommittedResource','ApplicationData::Current','NetworkInformation::GetInternetConnectionProfile','Gamepad::Gamepads','GetDefaultAudioRenderId']:
    if token not in text: print('ERROR missing probe token',token,file=sys.stderr); raise SystemExit(1)
if 'generated_code_execution' not in text or 'not executed' not in text.lower():
    print('ERROR generated-code M7 gate not explicit',file=sys.stderr); raise SystemExit(1)
data=json.loads(baseline.read_text(encoding="utf-8"))
allowed={'SUPPORTED','UNSUPPORTED','UNKNOWN','REQUIRES_HARDWARE_TEST'}
if data.get('run_state')!='NOT_RUN': raise SystemExit('ERROR baseline claims it ran')
for result in data['results']:
    if result['status'] not in allowed: raise SystemExit('ERROR invalid status')
    if result['status']=='SUPPORTED': raise SystemExit('ERROR baseline fabricates SUPPORTED capability')
print('PASS: capability probe project/XML/source coverage')
print('PASS: baseline xbox-capabilities.json is NOT_RUN and contains no fabricated SUPPORTED result')
