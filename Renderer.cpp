#include "Renderer.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace
{
[[nodiscard]] constexpr D3DCOLOR ToD3DColor(Color color) noexcept
{
    return D3DCOLOR_ARGB(color.a, color.r, color.g, color.b);
}

struct CustomVertex final
{
    constexpr CustomVertex(float xPosition, float yPosition, D3DCOLOR vertexColor) noexcept
        : x(xPosition), y(yPosition), color(vertexColor)
    {
    }

    float x;
    float y;
    float z{0.0F};
    float rhw{1.0F};
    D3DCOLOR color;
};

constexpr DWORD kCustomVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

[[nodiscard]] int TextLength(std::string_view message) noexcept
{
    constexpr auto maxLength = static_cast<std::size_t>((std::numeric_limits<int>::max)());
    return static_cast<int>((std::min)(message.size(), maxLength));
}
} // namespace

Renderer::Renderer(IDirect3DDevice9* device) : device_(device)
{
    if (device_ == nullptr)
    {
        throw std::invalid_argument("Renderer requires a valid Direct3D 9 device");
    }

    device_->AddRef();

    const HRESULT result = D3DXCreateLine(device_, &line_);
    if (FAILED(result))
    {
        device_->Release();
        device_ = nullptr;
        throw std::runtime_error("D3DXCreateLine failed");
    }
}

Renderer::~Renderer()
{
    InvalidateObjects();

    if (device_ != nullptr)
    {
        device_->Release();
        device_ = nullptr;
    }
}

HRESULT Renderer::OnLostDevice() noexcept
{
    hasSavedScissorState_ = false;
    return line_ != nullptr ? line_->OnLostDevice() : D3D_OK;
}

HRESULT Renderer::OnResetDevice() noexcept
{
    hasSavedScissorState_ = false;

    if (line_ != nullptr)
    {
        return line_->OnResetDevice();
    }

    return device_ != nullptr ? D3DXCreateLine(device_, &line_) : D3DERR_INVALIDCALL;
}

void Renderer::InvalidateObjects() noexcept
{
    if (line_ != nullptr)
    {
        line_->Release();
        line_ = nullptr;
    }
}

void Renderer::RenderLine(
    int x1,
    int y1,
    int x2,
    int y2,
    Color color,
    int width,
    bool antialias)
{
    if (line_ == nullptr)
    {
        return;
    }

    const D3DXVECTOR2 points[] = {
        {static_cast<float>(x1), static_cast<float>(y1)},
        {static_cast<float>(x2), static_cast<float>(y2)},
    };

    line_->SetAntialias(antialias ? TRUE : FALSE);
    line_->SetGLLines(FALSE);
    line_->SetWidth(static_cast<float>((std::max)(width, 1)));

    if (SUCCEEDED(line_->Begin()))
    {
        line_->Draw(points, 2, ToD3DColor(color));
        line_->End();
    }
}

void Renderer::RenderRect(
    int x1,
    int y1,
    int x2,
    int y2,
    Color color,
    int width,
    bool antialias)
{
    if (line_ == nullptr)
    {
        return;
    }

    const D3DXVECTOR2 points[] = {
        {static_cast<float>(x1), static_cast<float>(y1)},
        {static_cast<float>(x1), static_cast<float>(y2)},
        {static_cast<float>(x2), static_cast<float>(y2)},
        {static_cast<float>(x2), static_cast<float>(y1)},
        {static_cast<float>(x1), static_cast<float>(y1)},
    };

    line_->SetAntialias(antialias ? TRUE : FALSE);
    line_->SetGLLines(TRUE);
    line_->SetWidth(static_cast<float>((std::max)(width, 1)));

    if (SUCCEEDED(line_->Begin()))
    {
        line_->Draw(points, 5, ToD3DColor(color));
        line_->End();
    }
}

void Renderer::RenderFilledRect(int x1, int y1, int x2, int y2, Color color)
{
    if (device_ == nullptr)
    {
        return;
    }

    const auto d3dColor = ToD3DColor(color);
    const CustomVertex points[] = {
        {static_cast<float>(x1), static_cast<float>(y2), d3dColor},
        {static_cast<float>(x1), static_cast<float>(y1), d3dColor},
        {static_cast<float>(x2), static_cast<float>(y2), d3dColor},
        {static_cast<float>(x2), static_cast<float>(y1), d3dColor},
    };

    DWORD previousFvf = 0;
    const bool restoreFvf = SUCCEEDED(device_->GetFVF(&previousFvf));

    device_->SetFVF(kCustomVertexFvf);
    device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, points, sizeof(CustomVertex));

    if (restoreFvf)
    {
        device_->SetFVF(previousFvf);
    }
}

void Renderer::RenderGradient(
    int x1,
    int y1,
    int x2,
    int y2,
    Color first,
    Color second,
    bool horizontal)
{
    if (device_ == nullptr)
    {
        return;
    }

    const CustomVertex points[] = {
        {static_cast<float>(x1), static_cast<float>(y2), ToD3DColor(first)},
        {static_cast<float>(x1), static_cast<float>(y1), ToD3DColor(horizontal ? first : second)},
        {static_cast<float>(x2), static_cast<float>(y2), ToD3DColor(horizontal ? second : first)},
        {static_cast<float>(x2), static_cast<float>(y1), ToD3DColor(second)},
    };

    DWORD previousFvf = 0;
    const bool restoreFvf = SUCCEEDED(device_->GetFVF(&previousFvf));

    device_->SetFVF(kCustomVertexFvf);
    device_->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, points, sizeof(CustomVertex));

    if (restoreFvf)
    {
        device_->SetFVF(previousFvf);
    }
}

void Renderer::RenderText(
    std::string_view message,
    ID3DXFont* font,
    int x,
    int y,
    int x2,
    int y2,
    Color color,
    bool centered)
{
    if (font == nullptr || message.empty())
    {
        return;
    }

    RECT rectangle{x, y, x2, y2};
    DWORD format = DT_NOCLIP;

    if (centered && x2 > x && y2 > y)
    {
        format |= DT_CENTER | DT_VCENTER | DT_SINGLELINE;
    }

    font->DrawTextA(
        nullptr,
        message.data(),
        TextLength(message),
        &rectangle,
        format,
        ToD3DColor(color));
}

void Renderer::EnableScissorRect(int x1, int y1, int x2, int y2)
{
    if (device_ == nullptr)
    {
        return;
    }

    if (!hasSavedScissorState_)
    {
        if (FAILED(device_->GetRenderState(D3DRS_SCISSORTESTENABLE, &previousScissorEnabled_)) ||
            FAILED(device_->GetScissorRect(&previousScissorRect_)))
        {
            return;
        }

        hasSavedScissorState_ = true;
    }

    const RECT rectangle{x1, y1, x2, y2};
    device_->SetScissorRect(&rectangle);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
}

void Renderer::DisableScissorRect()
{
    if (device_ == nullptr)
    {
        return;
    }

    if (!hasSavedScissorState_)
    {
        return;
    }

    device_->SetScissorRect(&previousScissorRect_);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, previousScissorEnabled_);
    hasSavedScissorState_ = false;
}

void Renderer::RenderLine(Point from, Point to, Color color, int width, bool antialias)
{
    RenderLine(from.x, from.y, to.x, to.y, color, width, antialias);
}

void Renderer::RenderRect(Point from, Point to, Color color, int width, bool antialias)
{
    RenderRect(from.x, from.y, to.x, to.y, color, width, antialias);
}

void Renderer::RenderFilledRect(Point from, Point to, Color color)
{
    RenderFilledRect(from.x, from.y, to.x, to.y, color);
}

void Renderer::RenderGradient(
    Point from,
    Point to,
    Color first,
    Color second,
    bool horizontal)
{
    RenderGradient(from.x, from.y, to.x, to.y, first, second, horizontal);
}

void Renderer::RenderText(
    std::string_view message,
    ID3DXFont* font,
    Point from,
    Point to,
    Color color,
    bool centered)
{
    RenderText(message, font, from.x, from.y, to.x, to.y, color, centered);
}

void Renderer::EnableScissorRect(Point from, Point to)
{
    EnableScissorRect(from.x, from.y, to.x, to.y);
}

ID3DXFont* Renderer::CreateFont(
    const wchar_t* fontName,
    int size,
    bool bold,
    bool antialias) const noexcept
{
    if (device_ == nullptr || fontName == nullptr || size <= 0)
    {
        return nullptr;
    }

    ID3DXFont* font = nullptr;
    const HRESULT result = D3DXCreateFontW(
        device_,
        size,
        0,
        bold ? FW_BOLD : FW_NORMAL,
        1,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        antialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontName,
        &font);

    return SUCCEEDED(result) ? font : nullptr;
}

RECT Renderer::CalcTextSize(std::string_view message, ID3DXFont* font) const noexcept
{
    RECT rectangle{};

    if (font == nullptr || message.empty())
    {
        return rectangle;
    }

    font->DrawTextA(
        nullptr,
        message.data(),
        TextLength(message),
        &rectangle,
        DT_CALCRECT,
        D3DCOLOR_XRGB(0, 0, 0));

    return rectangle;
}
