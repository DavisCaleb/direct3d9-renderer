#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include "Menu.h"
#include "Renderer.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
constexpr wchar_t kOverlayClassName[] = L"Direct3D9OverlayExampleWindow";
constexpr COLORREF kTransparencyKey = RGB(0, 0, 0);

class OverlayApplication final
{
public:
    OverlayApplication(HINSTANCE instance, HWND target) noexcept
        : instance_(instance), target_(target)
    {
    }

    ~OverlayApplication()
    {
        font_ = Release(font_);
        renderer_.reset();
        device_ = Release(device_);
        direct3D_ = Release(direct3D_);

        if (window_ != nullptr && IsWindow(window_))
        {
            DestroyWindow(window_);
        }

        if (classRegistered_)
        {
            UnregisterClassW(kOverlayClassName, instance_);
        }
    }

    OverlayApplication(const OverlayApplication&) = delete;
    OverlayApplication& operator=(const OverlayApplication&) = delete;

    [[nodiscard]] HRESULT Initialize()
    {
        if (target_ == nullptr || !IsWindow(target_))
        {
            return E_INVALIDARG;
        }

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &OverlayApplication::WndProc;
        windowClass.cbWndExtra = sizeof(LONG_PTR);
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = kOverlayClassName;

        if (RegisterClassExW(&windowClass) == 0)
        {
            return LastErrorResult();
        }
        classRegistered_ = true;

        constexpr DWORD extendedStyle =
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        window_ = CreateWindowExW(
            extendedStyle,
            kOverlayClassName,
            L"Direct3D 9 Overlay Example",
            WS_POPUP,
            0,
            0,
            1,
            1,
            nullptr,
            nullptr,
            instance_,
            this);

        if (window_ == nullptr)
        {
            return LastErrorResult();
        }

        if (!SetLayeredWindowAttributes(window_, kTransparencyKey, 255, LWA_COLORKEY))
        {
            return LastErrorResult();
        }

        if (!SynchronizeTarget())
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
        }

        HRESULT result = CreateDevice();
        if (FAILED(result))
        {
            return result;
        }

        try
        {
            renderer_ = std::make_unique<Renderer>(device_);
        }
        catch (...)
        {
            return E_FAIL;
        }

        font_ = renderer_->CreateFont(L"Segoe UI", 17, false, true);
        if (font_ == nullptr)
        {
            return E_FAIL;
        }

        ConfigureMenu();
        return S_OK;
    }

    int Run()
    {
        MSG message{};

        while (window_ != nullptr && IsWindow(target_))
        {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    return static_cast<int>(message.wParam);
                }

                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            if (window_ == nullptr || !IsWindow(target_))
            {
                break;
            }

            if (!SynchronizeTarget())
            {
                break;
            }

            if (!targetIsActive_)
            {
                Sleep(16);
                continue;
            }

            menu_.UpdateInput();
            if (!EnsureDeviceReady())
            {
                Sleep(16);
                continue;
            }

            RenderFrame();
        }

        return 0;
    }

private:
    template <typename Interface>
    static Interface* Release(Interface* object) noexcept
    {
        if (object != nullptr)
        {
            object->Release();
        }
        return nullptr;
    }

    [[nodiscard]] static HRESULT LastErrorResult() noexcept
    {
        const DWORD error = GetLastError();
        return error != ERROR_SUCCESS ? HRESULT_FROM_WIN32(error) : E_FAIL;
    }

    void ConfigureMenu()
    {
        menu_.AddLabel("Overlay Demo");
        menu_.AddToggle("Status panel", showStatusPanel_);
        menu_.AddToggle("Center marker", showCenterMarker_);
        menu_.AddToggle("Corner guides", showCornerGuides_);
        menu_.AddIntSlider("Line width", lineWidth_, 1, 5);
        menu_.AddFloatSlider("Guide scale", guideScale_, 0.5F, 2.0F, 0.1F);
        menu_.AddColorPicker("Accent", accentColor_);
        menu_.AddToggle("Transparent menu", transparentMenu_);
    }

    [[nodiscard]] HRESULT CreateDevice() noexcept
    {
        direct3D_ = Direct3DCreate9(D3D_SDK_VERSION);
        if (direct3D_ == nullptr)
        {
            return E_FAIL;
        }

        presentationParameters_ = {};
        presentationParameters_.BackBufferWidth = width_;
        presentationParameters_.BackBufferHeight = height_;
        presentationParameters_.BackBufferFormat = D3DFMT_UNKNOWN;
        presentationParameters_.BackBufferCount = 1;
        presentationParameters_.MultiSampleType = D3DMULTISAMPLE_NONE;
        presentationParameters_.SwapEffect = D3DSWAPEFFECT_DISCARD;
        presentationParameters_.hDeviceWindow = window_;
        presentationParameters_.Windowed = TRUE;
        presentationParameters_.EnableAutoDepthStencil = FALSE;
        presentationParameters_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

        HRESULT result = direct3D_->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            window_,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &presentationParameters_,
            &device_);

        if (FAILED(result))
        {
            result = direct3D_->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                window_,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                &presentationParameters_,
                &device_);
        }

        return result;
    }

    [[nodiscard]] bool SynchronizeTarget() noexcept
    {
        if (target_ == nullptr || !IsWindow(target_))
        {
            return false;
        }

        RECT clientRectangle{};
        if (!GetClientRect(target_, &clientRectangle))
        {
            return false;
        }

        POINT topLeft{clientRectangle.left, clientRectangle.top};
        POINT bottomRight{clientRectangle.right, clientRectangle.bottom};
        if (!ClientToScreen(target_, &topLeft) || !ClientToScreen(target_, &bottomRight))
        {
            return false;
        }

        const int targetWidth = bottomRight.x - topLeft.x;
        const int targetHeight = bottomRight.y - topLeft.y;
        if (targetWidth <= 0 || targetHeight <= 0)
        {
            ShowWindow(window_, SW_HIDE);
            targetIsActive_ = false;
            return true;
        }

        width_ = static_cast<UINT>(targetWidth);
        height_ = static_cast<UINT>(targetHeight);

        const HWND foreground = GetForegroundWindow();
        const HWND foregroundRoot = foreground != nullptr ? GetAncestor(foreground, GA_ROOT) : nullptr;
        const HWND targetRoot = GetAncestor(target_, GA_ROOT);
        targetIsActive_ = !IsIconic(target_) && IsWindowVisible(target_) &&
                          (foreground == window_ || foregroundRoot == targetRoot);

        if (!targetIsActive_)
        {
            ShowWindow(window_, SW_HIDE);
            return true;
        }

        return SetWindowPos(
                   window_,
                   HWND_TOPMOST,
                   topLeft.x,
                   topLeft.y,
                   targetWidth,
                   targetHeight,
                   SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
    }

    [[nodiscard]] bool EnsureDeviceReady() noexcept
    {
        if (device_ == nullptr)
        {
            return false;
        }

        const HRESULT cooperativeLevel = device_->TestCooperativeLevel();
        if (cooperativeLevel == D3DERR_DEVICELOST)
        {
            return false;
        }
        if (cooperativeLevel == D3DERR_DEVICENOTRESET)
        {
            return SUCCEEDED(ResetDevice());
        }
        if (FAILED(cooperativeLevel))
        {
            return false;
        }

        if (presentationParameters_.BackBufferWidth != width_ ||
            presentationParameters_.BackBufferHeight != height_)
        {
            return SUCCEEDED(ResetDevice());
        }

        return true;
    }

    [[nodiscard]] HRESULT ResetDevice() noexcept
    {
        if (!resourcesAreLost_)
        {
            if (font_ != nullptr)
            {
                font_->OnLostDevice();
            }
            if (renderer_ != nullptr)
            {
                const HRESULT result = renderer_->OnLostDevice();
                (void)result;
            }
            resourcesAreLost_ = true;
        }

        presentationParameters_.BackBufferWidth = width_;
        presentationParameters_.BackBufferHeight = height_;
        const HRESULT result = device_->Reset(&presentationParameters_);
        if (FAILED(result))
        {
            return result;
        }

        HRESULT resourceResult = S_OK;
        if (font_ != nullptr)
        {
            resourceResult = font_->OnResetDevice();
        }
        if (renderer_ != nullptr)
        {
            const HRESULT rendererResult = renderer_->OnResetDevice();
            if (SUCCEEDED(resourceResult))
            {
                resourceResult = rendererResult;
            }
        }

        resourcesAreLost_ = false;
        return resourceResult;
    }

    void ConfigureRenderState() noexcept
    {
        device_->SetRenderState(D3DRS_ZENABLE, FALSE);
        device_->SetRenderState(D3DRS_LIGHTING, FALSE);
        device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }

    void RenderFrame()
    {
        UpdateFrameRate();
        device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0F, 0);
        ConfigureRenderState();

        if (FAILED(device_->BeginScene()))
        {
            return;
        }

        DrawOverlayContent();
        menu_.Render(*renderer_, font_, {24, 150}, transparentMenu_);

        device_->EndScene();
        device_->Present(nullptr, nullptr, nullptr, nullptr);
    }

    void DrawOverlayContent()
    {
        if (showStatusPanel_)
        {
            constexpr Point panelStart{24, 24};
            constexpr Point panelEnd{324, 126};
            renderer_->RenderFilledRect(panelStart, panelEnd, Color{22, 25, 33, 245});
            renderer_->RenderGradient(
                {24, 24},
                {324, 29},
                accentColor_,
                Color{75, 89, 190},
                true);
            renderer_->RenderRect(panelStart, panelEnd, Color{72, 78, 96}, 1, true);
            renderer_->RenderText(
                "Direct3D 9 Overlay",
                font_,
                Point{40, 43},
                Point{},
                Color{245, 246, 250});

            char details[96]{};
            std::snprintf(
                details,
                sizeof(details),
                "%u x %u  |  %.0f FPS",
                width_,
                height_,
                framesPerSecond_);
            renderer_->RenderText(
                details,
                font_,
                Point{40, 74},
                Point{},
                Color{165, 171, 188});
            renderer_->RenderText(
                "Press Insert to toggle controls",
                font_,
                Point{40, 97},
                Point{},
                accentColor_);
        }

        const int centerX = static_cast<int>(width_ / 2);
        const int centerY = static_cast<int>(height_ / 2);
        const int guideLength = static_cast<int>(12.0F * guideScale_);
        const int guideGap = static_cast<int>(5.0F * guideScale_);

        if (showCenterMarker_)
        {
            renderer_->RenderLine(
                centerX - guideGap - guideLength,
                centerY,
                centerX - guideGap,
                centerY,
                accentColor_,
                lineWidth_,
                true);
            renderer_->RenderLine(
                centerX + guideGap,
                centerY,
                centerX + guideGap + guideLength,
                centerY,
                accentColor_,
                lineWidth_,
                true);
            renderer_->RenderLine(
                centerX,
                centerY - guideGap - guideLength,
                centerX,
                centerY - guideGap,
                accentColor_,
                lineWidth_,
                true);
            renderer_->RenderLine(
                centerX,
                centerY + guideGap,
                centerX,
                centerY + guideGap + guideLength,
                accentColor_,
                lineWidth_,
                true);
        }

        if (showCornerGuides_)
        {
            DrawCornerGuide({20, 20}, {1, 1});
            DrawCornerGuide({static_cast<int>(width_) - 20, 20}, {-1, 1});
            DrawCornerGuide(
                {20, static_cast<int>(height_) - 20},
                {1, -1});
            DrawCornerGuide(
                {static_cast<int>(width_) - 20, static_cast<int>(height_) - 20},
                {-1, -1});
        }
    }

    void DrawCornerGuide(Point corner, Point direction)
    {
        const int length = static_cast<int>(28.0F * guideScale_);
        renderer_->RenderLine(
            corner,
            {corner.x + direction.x * length, corner.y},
            accentColor_,
            lineWidth_,
            true);
        renderer_->RenderLine(
            corner,
            {corner.x, corner.y + direction.y * length},
            accentColor_,
            lineWidth_,
            true);
    }

    void UpdateFrameRate() noexcept
    {
        ++frameCount_;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - frameRateStart_;
        if (elapsed.count() >= 1.0F)
        {
            framesPerSecond_ = static_cast<float>(frameCount_) / elapsed.count();
            frameCount_ = 0;
            frameRateStart_ = now;
        }
    }

    static LRESULT CALLBACK WndProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        auto* application = reinterpret_cast<OverlayApplication*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            application = static_cast<OverlayApplication*>(create->lpCreateParams);
            SetWindowLongPtrW(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(application));
        }

        switch (message)
        {
        case WM_NCHITTEST:
            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            BeginPaint(window, &paint);
            EndPaint(window, &paint);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            if (application != nullptr)
            {
                application->window_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    HINSTANCE instance_{nullptr};
    HWND target_{nullptr};
    HWND window_{nullptr};
    bool classRegistered_{false};
    bool targetIsActive_{false};

    IDirect3D9* direct3D_{nullptr};
    IDirect3DDevice9* device_{nullptr};
    D3DPRESENT_PARAMETERS presentationParameters_{};
    std::unique_ptr<Renderer> renderer_{};
    ID3DXFont* font_{nullptr};
    bool resourcesAreLost_{false};

    UINT width_{1};
    UINT height_{1};
    Menu menu_{"Overlay Controls"};
    bool showStatusPanel_{true};
    bool showCenterMarker_{true};
    bool showCornerGuides_{true};
    bool transparentMenu_{false};
    int lineWidth_{2};
    float guideScale_{1.0F};
    Color accentColor_{135, 145, 255};

    std::chrono::steady_clock::time_point frameRateStart_{
        std::chrono::steady_clock::now()};
    unsigned int frameCount_{0};
    float framesPerSecond_{0.0F};
};

[[nodiscard]] std::wstring ReadTargetTitle()
{
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr)
    {
        return {};
    }

    std::wstring title;
    if (argumentCount >= 2)
    {
        title = arguments[1];
    }

    LocalFree(arguments);
    return title;
}
} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    SetProcessDPIAware();

    const std::wstring targetTitle = ReadTargetTitle();
    if (targetTitle.empty())
    {
        MessageBoxW(
            nullptr,
            L"Usage:\n  direct3d9-overlay-example.exe \"Target Window Title\"",
            L"Direct3D 9 Overlay Example",
            MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    const HWND target = FindWindowW(nullptr, targetTitle.c_str());
    if (target == nullptr)
    {
        MessageBoxW(
            nullptr,
            L"The requested target window was not found.",
            L"Direct3D 9 Overlay Example",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    OverlayApplication application(instance, target);
    const HRESULT result = application.Initialize();
    if (FAILED(result))
    {
        MessageBoxW(
            nullptr,
            L"The Direct3D 9 overlay could not be initialized.",
            L"Direct3D 9 Overlay Example",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    return application.Run();
}
