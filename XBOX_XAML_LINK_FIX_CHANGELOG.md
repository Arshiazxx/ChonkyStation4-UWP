# Xbox C++/CX XAML linker fix

Changed:
- `platform/xbox/ChonkyStation4.Xbox/App.xaml.cpp`
  - replaced `#include "App.g.h"` with `#include "App.g.hpp"`
- `platform/xbox/ChonkyStation4.Xbox/MainPage.xaml.cpp`
  - replaced `#include "MainPage.g.h"` with `#include "MainPage.g.hpp"`

Reason:
The generated `.g.h` files provide declarations, while the generated `.g.hpp` files contain the C++/CX XAML implementation glue required for `InitializeComponent`, XAML metadata provider/component connector methods, and the generated UWP application entry point.

Verification command on Windows:

```powershell
msbuild .\ChonkyStation4.Xbox.vcxproj /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
```
