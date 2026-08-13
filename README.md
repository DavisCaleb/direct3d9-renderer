# Direct3D 9 Renderer

A compact C++17 immediate-mode 2D rendering helper for legacy Direct3D 9 applications. It wraps common overlay and UI drawing operations while keeping device and resource ownership explicit.

## Features

- Antialiased lines and outlined rectangles
- Solid and two-color gradient rectangles
- ANSI text drawing and text measurement with `ID3DXFont`
- Scoped scissor rectangle support that restores the previous device state
- Direct3D device-loss and reset handling
- Small, dependency-light API with `Point` and `Color` value types

## Requirements

- Windows
- A C++17 compiler (Visual Studio 2019 or newer recommended)
- Direct3D 9
- The legacy D3DX9 headers and library

> [!NOTE]
> D3DX is deprecated and is not included in modern Windows SDKs. Install the [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812), or provide an equivalent D3DX9 development package. The CMake build checks `DXSDK_DIR` when locating it.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

This produces the `d3d9_renderer` static library. Consumers can also add this repository with `add_subdirectory` and link against `d3d9_renderer`.

## Example

Drawing must happen between the application's `BeginScene` and `EndScene` calls.

```cpp
#include "Renderer.h"

// `device` is an initialized IDirect3DDevice9 pointer.
Renderer renderer(device);

ID3DXFont* font = renderer.CreateFont(L"Segoe UI", 18, true, true);

renderer.RenderFilledRect({20, 20}, {280, 120}, Color{24, 28, 36, 230});
renderer.RenderGradient({20, 120}, {280, 126}, Color{64, 150, 255}, Color{126, 87, 255}, true);
renderer.RenderRect({20, 20}, {280, 126}, Color{220, 225, 235}, 1, true);
renderer.RenderText("Direct3D 9 Renderer", font, {32, 34}, {}, Color{255, 255, 255});

if (font != nullptr)
{
    font->Release();
}
```

For centered text, provide both corners of a non-empty rectangle:

```cpp
renderer.RenderText(
    "Centered",
    font,
    Point{20, 20},
    Point{280, 120},
    Color{255, 255, 255},
    true);
```

## Device lifecycle

Call the renderer hooks around a Direct3D device reset. Fonts are created for the caller, so the caller remains responsible for forwarding the same notifications to each font.

```cpp
const HRESULT rendererLost = renderer.OnLostDevice();
const HRESULT fontLost = font->OnLostDevice();

// Check the results, then call device->Reset(...).

const HRESULT fontReset = font->OnResetDevice();
const HRESULT rendererReset = renderer.OnResetDevice();
```

Check the returned `HRESULT` values in production code. The renderer retains the device through COM reference counting and releases it when destroyed. A font returned by `CreateFont` is caller-owned and must be released.

## Design notes

- This library does not call `BeginScene`, `EndScene`, or `Present`; frame ownership stays with the host application.
- Filled and gradient draws temporarily change the fixed-function vertex format and restore it afterward.
- Blending, sampler configuration, and other pipeline state remain under host-application control.
- `EnableScissorRect` saves the current scissor rectangle and enabled state. `DisableScissorRect` restores both.
- The renderer is intentionally not thread-safe, matching typical Direct3D 9 rendering usage.

## Repository layout

```text
.
├── CMakeLists.txt  # Portable project configuration for supported Windows toolchains
├── Renderer.cpp    # Rendering implementation
├── Renderer.h      # Public API and API documentation
└── README.md       # Setup, usage, and lifecycle documentation
```

## License

Released under the MIT License. See [LICENSE](LICENSE).
