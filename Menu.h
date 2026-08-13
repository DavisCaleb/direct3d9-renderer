#pragma once

#include "Renderer.h"

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

/** A named boolean option displayed by a multi-select menu item. */
struct MenuOption final
{
    MenuOption(std::string optionName, bool& selectedValue)
        : name(std::move(optionName)), selected(selectedValue)
    {
    }

    std::string name;
    std::reference_wrapper<bool> selected;
};

/**
 * Keyboard-driven menu rendered with Renderer.
 *
 * Bound values and the font passed to Render are non-owning references. They
 * must remain valid while the menu uses them. Call UpdateInput once per frame
 * and Render between the application's BeginScene and EndScene calls.
 */
class Menu final
{
public:
    explicit Menu(std::string title = "Keyboard Menu");

    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu(Menu&&) noexcept = default;
    Menu& operator=(Menu&&) noexcept = default;
    ~Menu() = default;

    void AddToggle(std::string name, bool& value);
    void AddIntSlider(std::string name, int& value, int minimum, int maximum, int step = 1);
    void AddFloatSlider(
        std::string name,
        float& value,
        float minimum,
        float maximum,
        float step = 1.0F);
    void AddLabel(std::string name);
    void AddSpacer();
    void AddKeybind(std::string name, int& virtualKey);
    void AddMultiSelect(std::string name, std::vector<MenuOption> options);
    void AddColorPicker(std::string name, Color& color);

    /** Poll Win32 keyboard state and update the selected item. */
    void UpdateInput();

    /**
     * Draw the menu with a caller-owned D3DX font.
     *
     * @param renderer Renderer associated with the active Direct3D 9 device.
     * @param font Valid font used for all menu text.
     * @param position Upper-left menu position in screen coordinates.
     * @param transparent Skip panel backgrounds when true.
     */
    void Render(
        Renderer& renderer,
        ID3DXFont* font,
        Point position,
        bool transparent = false) const;

    void SetVisible(bool visible) noexcept;
    [[nodiscard]] bool IsVisible() const noexcept;
    [[nodiscard]] bool Empty() const noexcept;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    enum class ItemType
    {
        Toggle,
        IntSlider,
        FloatSlider,
        Label,
        Keybind,
        MultiSelect,
        ColorPicker,
        Spacer,
    };

    struct ToggleBinding final
    {
        std::reference_wrapper<bool> value;
    };

    struct IntSliderBinding final
    {
        std::reference_wrapper<int> value;
        int minimum;
        int maximum;
        int step;
    };

    struct FloatSliderBinding final
    {
        std::reference_wrapper<float> value;
        float minimum;
        float maximum;
        float step;
    };

    struct KeybindBinding final
    {
        std::reference_wrapper<int> virtualKey;
    };

    struct MultiSelectBinding final
    {
        std::vector<MenuOption> options;
        std::size_t optionIndex{0};
    };

    struct ColorBinding final
    {
        std::reference_wrapper<Color> color;
        std::size_t channel{0};
    };

    using Binding = std::variant<
        std::monostate,
        ToggleBinding,
        IntSliderBinding,
        FloatSliderBinding,
        KeybindBinding,
        MultiSelectBinding,
        ColorBinding>;

    struct Item final
    {
        ItemType type;
        std::string name;
        Binding binding;
    };

    using KeyState = std::array<bool, 256>;

    [[nodiscard]] KeyState PollPressedKeys() noexcept;
    [[nodiscard]] bool HasSelectableItem() const noexcept;
    [[nodiscard]] bool IsSelectable(std::size_t index) const noexcept;
    void NormalizeSelection() noexcept;
    void MoveSelection(int direction) noexcept;
    void ProcessCurrentItem(const KeyState& pressed);
    void ProcessKeyCapture(const KeyState& pressed);
    void ProcessColorEditor(const KeyState& pressed);

    void RenderItem(
        Renderer& renderer,
        ID3DXFont* font,
        const Item& item,
        std::size_t index,
        Point position) const;

    std::string title_;
    std::vector<Item> items_;
    std::size_t currentIndex_{0};
    bool visible_{true};
    bool editingColor_{false};
    bool capturingKey_{false};
    KeyState previousKeyState_{};
};
