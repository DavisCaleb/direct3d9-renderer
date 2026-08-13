#pragma once

#include <cstdint>
#include <string_view>

#include <d3d9.h>
#include <d3dx9.h>

/** RGBA color with 8-bit channels. */
struct Color final
{
    constexpr Color() noexcept = default;

    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        : r(red), g(green), b(blue)
    {
    }

    constexpr Color(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue,
        std::uint8_t alpha) noexcept
        : r(red), g(green), b(blue), a(alpha)
    {
    }

    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};
};

/** Integer point in screen space. */
struct Point final
{
    constexpr Point() noexcept = default;
    constexpr Point(int xPosition, int yPosition) noexcept : x(xPosition), y(yPosition) {}

    [[nodiscard]] constexpr Point operator+(const Point& other) const noexcept
    {
        return {x + other.x, y + other.y};
    }

    [[nodiscard]] constexpr Point operator-(const Point& other) const noexcept
    {
        return {x - other.x, y - other.y};
    }

    [[nodiscard]] constexpr Point operator+(int value) const noexcept
    {
        return {x + value, y + value};
    }

    [[nodiscard]] constexpr Point operator-(int value) const noexcept
    {
        return {x - value, y - value};
    }

    /** The divisor must not be zero. */
    [[nodiscard]] constexpr Point operator/(int divisor) const noexcept
    {
        return {x / divisor, y / divisor};
    }

    constexpr Point& operator+=(const Point& other) noexcept
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Point& operator-=(const Point& other) noexcept
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    int x{0};
    int y{0};
};

/**
 * Lightweight immediate-mode 2D renderer for Direct3D 9.
 *
 * The renderer retains the supplied device with AddRef and releases it on
 * destruction. Draw calls must be made between the application's BeginScene
 * and EndScene calls. The class is not thread-safe.
 */
class Renderer final
{
public:
    /**
     * Creates a renderer for a valid Direct3D 9 device.
     *
     * @throws std::invalid_argument if device is null.
     * @throws std::runtime_error if the D3DX line helper cannot be created.
     */
    explicit Renderer(IDirect3DDevice9* device);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /** Forward device-loss notification to owned D3DX resources. */
    [[nodiscard]] HRESULT OnLostDevice() noexcept;

    /** Forward device-reset notification or recreate invalidated resources. */
    [[nodiscard]] HRESULT OnResetDevice() noexcept;

    /** Release device-dependent helper objects. Drawing is disabled until reset. */
    void InvalidateObjects() noexcept;

    void RenderLine(
        int x1,
        int y1,
        int x2,
        int y2,
        Color color,
        int width = 1,
        bool antialias = false);
    void RenderRect(
        int x1,
        int y1,
        int x2,
        int y2,
        Color color,
        int width = 1,
        bool antialias = false);
    void RenderFilledRect(int x1, int y1, int x2, int y2, Color color);

    /** Draw a two-color gradient; vertical by default. */
    void RenderGradient(
        int x1,
        int y1,
        int x2,
        int y2,
        Color first,
        Color second,
        bool horizontal = false);

    /**
     * Draw text at (x, y). Supply a valid lower-right corner and set centered
     * to true to center the text in that rectangle.
     */
    void RenderText(
        std::string_view message,
        ID3DXFont* font,
        int x,
        int y,
        int x2 = 0,
        int y2 = 0,
        Color color = {},
        bool centered = false);

    /** Save the current scissor state and enable the supplied clipping area. */
    void EnableScissorRect(int x1, int y1, int x2, int y2);

    /** Restore the scissor state captured by EnableScissorRect. */
    void DisableScissorRect();

    void RenderLine(
        Point from,
        Point to,
        Color color,
        int width = 1,
        bool antialias = false);
    void RenderRect(
        Point from,
        Point to,
        Color color,
        int width = 1,
        bool antialias = false);
    void RenderFilledRect(Point from, Point to, Color color);
    void RenderGradient(
        Point from,
        Point to,
        Color first,
        Color second,
        bool horizontal = false);
    void RenderText(
        std::string_view message,
        ID3DXFont* font,
        Point from,
        Point to = {},
        Color color = {},
        bool centered = false);
    void EnableScissorRect(Point from, Point to);

    /**
     * Create a D3DX font. The caller owns the returned COM object and must call
     * Release. Returns nullptr when font creation fails.
     */
    [[nodiscard]] ID3DXFont* CreateFont(
        const wchar_t* fontName,
        int size,
        bool bold = false,
        bool antialias = false) const noexcept;

    /** Calculate the bounding rectangle of a text string. */
    [[nodiscard]] RECT CalcTextSize(std::string_view message, ID3DXFont* font) const noexcept;

private:
    IDirect3DDevice9* device_{nullptr};
    ID3DXLine* line_{nullptr};

    RECT previousScissorRect_{};
    DWORD previousScissorEnabled_{FALSE};
    bool hasSavedScissorState_{false};
};
