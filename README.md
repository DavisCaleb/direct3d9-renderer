# Direct3D 9 Renderer and Keyboard Menu

A compact C++17 2D renderer and keyboard-driven settings menu for legacy Direct3D 9 applications. The project was originally created in **2021** and has since been cleaned up, documented, and packaged for reuse.

## Features

### Renderer

- Antialiased lines and outlined rectangles
- Solid and two-color gradient rectangles
- Text drawing and measurement with `ID3DXFont`
- Scissor clipping that restores the previous device state
- Direct3D device-loss and reset handling
- Explicit COM ownership and a small `Point`/`Color` API

### Keyboard menu

- Toggles
- Integer and floating-point sliders
- Key bindings with direct key capture
- Multi-select options
- RGB color editing
- Labels, spacers, selection highlighting, and an optional transparent layout
- Edge-triggered Win32 input, so actions occur once per key press

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

This produces the `d3d9_renderer` static library. Applications using CMake can also include the repository with `add_subdirectory` and link against `Direct3D9Renderer::Renderer`.

## Renderer example

Drawing must happen between the application's `BeginScene` and `EndScene` calls.

```cpp
#include "Renderer.h"

// `device` is an initialized IDirect3DDevice9 pointer.
Renderer renderer(device);
ID3DXFont* font = renderer.CreateFont(L"Segoe UI", 18, true, true);

renderer.RenderFilledRect({20, 20}, {280, 120}, Color{24, 28, 36, 230});
renderer.RenderGradient(
    {20, 120},
    {280, 126},
    Color{64, 150, 255},
    Color{126, 87, 255},
    true);
renderer.RenderRect({20, 20}, {280, 126}, Color{220, 225, 235}, 1, true);
renderer.RenderText(
    "Direct3D 9 Renderer",
    font,
    Point{32, 34},
    Point{},
    Color{255, 255, 255});
```

## Menu example

The menu stores non-owning references to application settings, so those settings must outlive the menu.

```cpp
#include "Menu.h"

bool enabled = true;
bool bloom = true;
bool colorCorrection = false;
int quality = 3;
int activationKey = VK_F1;
float opacity = 0.85F;
Color accent{135, 145, 255};

Menu menu("Renderer Settings");
menu.AddLabel("Display");
menu.AddToggle("Enabled", enabled);
menu.AddIntSlider("Quality", quality, 1, 5);
menu.AddFloatSlider("Opacity", opacity, 0.0F, 1.0F, 0.05F);
menu.AddMultiSelect(
    "Effects",
    {{"Bloom", bloom}, {"Color correction", colorCorrection}});
menu.AddColorPicker("Accent", accent);
menu.AddKeybind("Activation key", activationKey);
```

Update input once per frame, then render during the active scene:

```cpp
menu.UpdateInput();

if (SUCCEEDED(device->BeginScene()))
{
    menu.Render(renderer, font, {30, 30});
    device->EndScene();
}
```

### Controls

| Key | Action |
| --- | --- |
| `Insert` | Show or hide the menu |
| `Up` / `Down` | Change the selected item |
| `Left` / `Right` | Adjust a value or choose an option/channel |
| `Enter` | Toggle, activate key capture, or enter/leave color editing |
| `Escape` | Cancel key capture or leave color editing |
| `Backspace` / `Delete` | Clear the selected key binding |

While editing a color, use `Left`/`Right` to select the RGB channel and `Up`/`Down` to change its value.

## Device lifecycle

Call the renderer hooks around a Direct3D device reset. Fonts are caller-owned and require the same lifecycle notifications.

```cpp
const HRESULT rendererLost = renderer.OnLostDevice();
const HRESULT fontLost = font->OnLostDevice();

// Check the results, then call device->Reset(...).

const HRESULT fontReset = font->OnResetDevice();
const HRESULT rendererReset = renderer.OnResetDevice();
```

Check the returned `HRESULT` values in production code. The renderer retains the device through COM reference counting and releases it when destroyed. A font returned by `CreateFont` must be released by its caller.

```cpp
if (font != nullptr)
{
    font->Release();
}
```

## Design notes

- The library does not call `BeginScene`, `EndScene`, or `Present`; frame ownership remains with the host application.
- Filled and gradient draws temporarily change the fixed-function vertex format and restore it afterward.
- Blending, sampler configuration, and other pipeline state remain under host-application control.
- The menu uses `GetAsyncKeyState` and is intended for a standard single-threaded Windows render loop.
- The renderer and menu are not thread-safe.

## Repository layout

```text
.
├── CMakeLists.txt  # Windows library build
├── Renderer.h/.cpp # Direct3D 9 rendering API and implementation
├── Menu.h/.cpp     # Keyboard menu API and implementation
├── README.md       # Setup, usage, controls, and lifecycle documentation
└── LICENSE         # MIT License
```

## License

Released under the MIT License. See [LICENSE](LICENSE).
