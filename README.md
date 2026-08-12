<h1>
  <div align="center">
    <strong>ChonkyStation4 Xbox/UWP Port</strong>
  </div>
</h1>

---

<p align="center">
ChonkyStation4 Xbox/UWP is an experimental PlayStation 4 emulator port targeting Xbox Dev Mode.<br>
This project brings the ChonkyStation4 emulator architecture to the Universal Windows Platform while building the foundation required for future PS4 emulation on Xbox hardware.<br>
<br>
This is an open-source hobby project created for research, experimentation, and learning.
</p>

---

<div align="center">

🎮 Xbox Dev Mode Support  
🖥️ UWP Application  
⚡ Direct3D12 Backend Foundation  
🧠 Emulator Core Development  

</div>

---

# Current Status

ChonkyStation4 Xbox/UWP is currently in early development.

## Implemented

✅ Xbox UWP application layer  
✅ Xbox Dev Mode deployment  
✅ MSIX packaging  
✅ Controller input support  
✅ Xbox lifecycle handling  
✅ Logging system  
✅ Storage integration  
✅ D3D12 rendering foundation  
✅ Platform-independent emulator core  
✅ ELF64 loader foundation  
✅ Guest virtual memory abstraction  
✅ Test ELF loading pipeline  

---

# Emulator Core

The project is currently focused on building the core systems required for PS4 emulation.

Current development areas:

```
Core/
├── Loader/
│   └── ELF64 loading foundation
│
├── Memory/
│   └── Guest virtual memory
│
├── CPU/
│   └── Execution framework (in development)
│
├── Kernel/
│   └── Planned
│
├── GPU/
│   └── D3D12 translation foundation
│
└── FileSystem/
    └── Planned
```

---

# Installation

## Requirements

- Xbox console with Developer Mode enabled
- Windows PC
- Xbox Device Portal access

## Installing

1. Build the project using Visual Studio/MSBuild.
2. Generate the MSIX package.
3. Install the package through Xbox Device Portal.
4. Launch ChonkyStation4 from Developer Mode.

---

# Development

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/ChonkyStation4.git
```

Build Xbox project:

```powershell
msbuild .\platform\xbox\ChonkyStation4.Xbox.sln /m /p:Configuration=Release /p:Platform=x64
```

---

# Testing

The current debug build includes experimental developer tools.

Current test functionality:

- Load test ELF files
- Validate ELF64 structure
- Map guest memory segments
- Display loader information

Example:

```
ChonkyStation4 Loader

Format:
ELF64

Architecture:
x86-64

Entry Point:
0x400000

Segments:
5

Memory Mapping:
Success
```

---

# Compatibility

⚠️ Commercial PS4 game compatibility is currently unavailable.

The project is still developing the required systems:

- CPU execution
- Orbis kernel functionality
- PS4 system libraries
- GPU command translation
- Runtime services

---

# Roadmap

## Completed

✅ Xbox/UWP foundation  
✅ Emulator core structure  
✅ ELF loader foundation  
✅ Guest memory system  

## In Progress

🚧 CPU execution framework  
🚧 Instruction dispatch  
🚧 Emulator runtime environment  

## Future

⬜ Orbis OS compatibility layer  
⬜ PS4 system call implementation  
⬜ GPU emulation  
⬜ Homebrew execution  
⬜ Commercial game compatibility  

---

# Disclaimer

ChonkyStation4 Xbox/UWP is an independent research project.

This project does not include copyrighted PlayStation 4 software, firmware, or game files.

Users are responsible for providing their own legally obtained files.

---

# Credits

Original ChonkyStation4 project:

https://github.com/liuk7071/ChonkyStation4

Xbox/UWP port maintained by the community.

---

<div align="center">

⭐ Star the project if you are interested in Xbox emulation research!

</div>
