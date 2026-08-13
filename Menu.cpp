#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Menu.h"

#include <windows.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
constexpr int kMenuWidth = 300;
constexpr int kHeaderHeight = 32;
constexpr int kRowHeight = 22;
constexpr int kPadding = 9;

constexpr Color kPanelColor{24, 27, 34, 245};
constexpr Color kHeaderColor{38, 42, 54, 255};
constexpr Color kBorderColor{76, 82, 101, 255};
constexpr Color kSelectedColor{50, 57, 78, 255};
constexpr Color kAccentColor{135, 145, 255, 255};
constexpr Color kTextColor{235, 237, 242, 255};
constexpr Color kMutedColor{155, 160, 174, 255};
constexpr Color kEnabledColor{127, 210, 156, 255};
constexpr Color kDisabledColor{232, 116, 116, 255};
constexpr Color kShadowColor{5, 6, 8, 230};

[[nodiscard]] bool IsPressed(const std::array<bool, 256>& pressed, int virtualKey) noexcept
{
    return virtualKey >= 0 && virtualKey < static_cast<int>(pressed.size()) &&
           pressed[static_cast<std::size_t>(virtualKey)];
}

void DrawOutlinedText(
    Renderer& renderer,
    ID3DXFont* font,
    std::string_view text,
    Point from,
    Color color,
    bool centered = false,
    Point to = {})
{
    constexpr Point offsets[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    for (const Point offset : offsets)
    {
        renderer.RenderText(text, font, from + offset, to + offset, kShadowColor, centered);
    }

    renderer.RenderText(text, font, from, to, color, centered);
}

[[nodiscard]] int TextWidth(Renderer& renderer, ID3DXFont* font, std::string_view text) noexcept
{
    const RECT bounds = renderer.CalcTextSize(text, font);
    return static_cast<int>(bounds.right - bounds.left);
}

[[nodiscard]] std::string VirtualKeyName(int virtualKey)
{
    if (virtualKey <= 0)
    {
        return "Unbound";
    }

    switch (virtualKey)
    {
    case VK_LBUTTON:
        return "Left Mouse";
    case VK_RBUTTON:
        return "Right Mouse";
    case VK_MBUTTON:
        return "Middle Mouse";
    case VK_XBUTTON1:
        return "Mouse 4";
    case VK_XBUTTON2:
        return "Mouse 5";
    default:
        break;
    }

    UINT scanCode = MapVirtualKeyA(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC);
    LONG keyData = static_cast<LONG>(scanCode << 16U);

    switch (virtualKey)
    {
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
    case VK_DIVIDE:
        keyData |= 1L << 24;
        break;
    default:
        break;
    }

    char name[64]{};
    if (GetKeyNameTextA(keyData, name, static_cast<int>(sizeof(name))) > 0)
    {
        return name;
    }

    return "VK " + std::to_string(virtualKey);
}

[[nodiscard]] std::uint8_t AdjustChannel(std::uint8_t value, int direction) noexcept
{
    const int adjusted = (static_cast<int>(value) + direction + 256) % 256;
    return static_cast<std::uint8_t>(adjusted);
}
} // namespace

Menu::Menu(std::string title) : title_(std::move(title))
{
}

void Menu::AddToggle(std::string name, bool& value)
{
    items_.push_back({ItemType::Toggle, std::move(name), ToggleBinding{value}});
    NormalizeSelection();
}

void Menu::AddIntSlider(
    std::string name,
    int& value,
    int minimum,
    int maximum,
    int step)
{
    if (minimum > maximum)
    {
        throw std::invalid_argument("integer slider minimum must not exceed maximum");
    }
    if (step <= 0)
    {
        throw std::invalid_argument("integer slider step must be positive");
    }

    value = (std::clamp)(value, minimum, maximum);
    items_.push_back(
        {ItemType::IntSlider, std::move(name), IntSliderBinding{value, minimum, maximum, step}});
    NormalizeSelection();
}

void Menu::AddFloatSlider(
    std::string name,
    float& value,
    float minimum,
    float maximum,
    float step)
{
    if (minimum > maximum)
    {
        throw std::invalid_argument("float slider minimum must not exceed maximum");
    }
    if (step <= 0.0F)
    {
        throw std::invalid_argument("float slider step must be positive");
    }

    value = (std::clamp)(value, minimum, maximum);
    items_.push_back(
        {ItemType::FloatSlider, std::move(name), FloatSliderBinding{value, minimum, maximum, step}});
    NormalizeSelection();
}

void Menu::AddLabel(std::string name)
{
    items_.push_back({ItemType::Label, std::move(name), std::monostate{}});
    NormalizeSelection();
}

void Menu::AddSpacer()
{
    items_.push_back({ItemType::Spacer, {}, std::monostate{}});
    NormalizeSelection();
}

void Menu::AddKeybind(std::string name, int& virtualKey)
{
    virtualKey = (std::clamp)(virtualKey, 0, 255);
    items_.push_back({ItemType::Keybind, std::move(name), KeybindBinding{virtualKey}});
    NormalizeSelection();
}

void Menu::AddMultiSelect(std::string name, std::vector<MenuOption> options)
{
    if (options.empty())
    {
        throw std::invalid_argument("multi-select items require at least one option");
    }

    items_.push_back(
        {ItemType::MultiSelect, std::move(name), MultiSelectBinding{std::move(options)}});
    NormalizeSelection();
}

void Menu::AddColorPicker(std::string name, Color& color)
{
    items_.push_back({ItemType::ColorPicker, std::move(name), ColorBinding{color}});
    NormalizeSelection();
}

void Menu::UpdateInput()
{
    const KeyState pressed = PollPressedKeys();

    if (IsPressed(pressed, VK_INSERT))
    {
        visible_ = !visible_;
        editingColor_ = false;
        capturingKey_ = false;
        return;
    }

    if (!visible_ || !HasSelectableItem())
    {
        return;
    }

    NormalizeSelection();

    if (capturingKey_)
    {
        ProcessKeyCapture(pressed);
        return;
    }

    if (editingColor_)
    {
        ProcessColorEditor(pressed);
        return;
    }

    if (IsPressed(pressed, VK_DOWN))
    {
        MoveSelection(1);
    }
    else if (IsPressed(pressed, VK_UP))
    {
        MoveSelection(-1);
    }

    ProcessCurrentItem(pressed);
}

void Menu::Render(
    Renderer& renderer,
    ID3DXFont* font,
    Point position,
    bool transparent) const
{
    if (!visible_ || font == nullptr)
    {
        return;
    }

    const int panelHeight = kHeaderHeight + static_cast<int>(items_.size()) * kRowHeight;
    const Point panelEnd{position.x + kMenuWidth, position.y + panelHeight};

    if (!transparent)
    {
        renderer.RenderFilledRect(position, panelEnd, kPanelColor);
        renderer.RenderFilledRect(
            position,
            {position.x + kMenuWidth, position.y + kHeaderHeight},
            kHeaderColor);
        renderer.RenderRect(position, panelEnd, kBorderColor);
    }

    DrawOutlinedText(
        renderer,
        font,
        title_,
        {position.x + kPadding, position.y + 7},
        kTextColor);

    for (std::size_t index = 0; index < items_.size(); ++index)
    {
        const Point itemPosition{
            position.x,
            position.y + kHeaderHeight + static_cast<int>(index) * kRowHeight,
        };
        RenderItem(renderer, font, items_[index], index, itemPosition);
    }
}

void Menu::SetVisible(bool visible) noexcept
{
    visible_ = visible;
    if (!visible_)
    {
        editingColor_ = false;
        capturingKey_ = false;
    }
}

bool Menu::IsVisible() const noexcept
{
    return visible_;
}

bool Menu::Empty() const noexcept
{
    return items_.empty();
}

std::size_t Menu::Size() const noexcept
{
    return items_.size();
}

Menu::KeyState Menu::PollPressedKeys() noexcept
{
    KeyState pressed{};

    for (std::size_t key = 0; key < pressed.size(); ++key)
    {
        const bool isDown = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
        pressed[key] = isDown && !previousKeyState_[key];
        previousKeyState_[key] = isDown;
    }

    return pressed;
}

bool Menu::HasSelectableItem() const noexcept
{
    for (std::size_t index = 0; index < items_.size(); ++index)
    {
        if (IsSelectable(index))
        {
            return true;
        }
    }

    return false;
}

bool Menu::IsSelectable(std::size_t index) const noexcept
{
    if (index >= items_.size())
    {
        return false;
    }

    return items_[index].type != ItemType::Label && items_[index].type != ItemType::Spacer;
}

void Menu::NormalizeSelection() noexcept
{
    if (IsSelectable(currentIndex_))
    {
        return;
    }

    for (std::size_t index = 0; index < items_.size(); ++index)
    {
        if (IsSelectable(index))
        {
            currentIndex_ = index;
            return;
        }
    }

    currentIndex_ = 0;
}

void Menu::MoveSelection(int direction) noexcept
{
    if (items_.empty() || direction == 0)
    {
        return;
    }

    const auto itemCount = static_cast<std::ptrdiff_t>(items_.size());
    auto index = static_cast<std::ptrdiff_t>(currentIndex_);

    for (std::size_t attempt = 0; attempt < items_.size(); ++attempt)
    {
        index = (index + direction + itemCount) % itemCount;
        if (IsSelectable(static_cast<std::size_t>(index)))
        {
            currentIndex_ = static_cast<std::size_t>(index);
            return;
        }
    }
}

void Menu::ProcessCurrentItem(const KeyState& pressed)
{
    Item& item = items_[currentIndex_];
    const bool decrease = IsPressed(pressed, VK_LEFT);
    const bool increase = IsPressed(pressed, VK_RIGHT);
    const bool activate = IsPressed(pressed, VK_RETURN);

    switch (item.type)
    {
    case ItemType::Toggle:
    {
        if (decrease || increase || activate)
        {
            auto& value = std::get<ToggleBinding>(item.binding).value.get();
            value = !value;
        }
        break;
    }
    case ItemType::IntSlider:
    {
        auto& binding = std::get<IntSliderBinding>(item.binding);
        const auto value = static_cast<long long>(binding.value.get());
        if (decrease)
        {
            binding.value.get() = static_cast<int>((std::max)(
                value - binding.step,
                static_cast<long long>(binding.minimum)));
        }
        else if (increase)
        {
            binding.value.get() = static_cast<int>((std::min)(
                value + binding.step,
                static_cast<long long>(binding.maximum)));
        }
        break;
    }
    case ItemType::FloatSlider:
    {
        auto& binding = std::get<FloatSliderBinding>(item.binding);
        if (decrease)
        {
            binding.value.get() =
                (std::max)(binding.value.get() - binding.step, binding.minimum);
        }
        else if (increase)
        {
            binding.value.get() =
                (std::min)(binding.value.get() + binding.step, binding.maximum);
        }
        break;
    }
    case ItemType::Keybind:
    {
        auto& virtualKey = std::get<KeybindBinding>(item.binding).virtualKey.get();
        if (decrease)
        {
            virtualKey = (virtualKey + 255) % 256;
        }
        else if (increase)
        {
            virtualKey = (virtualKey + 1) % 256;
        }
        else if (activate)
        {
            capturingKey_ = true;
        }
        else if (IsPressed(pressed, VK_BACK) || IsPressed(pressed, VK_DELETE))
        {
            virtualKey = 0;
        }
        break;
    }
    case ItemType::MultiSelect:
    {
        auto& binding = std::get<MultiSelectBinding>(item.binding);
        if (decrease)
        {
            binding.optionIndex =
                (binding.optionIndex + binding.options.size() - 1) % binding.options.size();
        }
        else if (increase)
        {
            binding.optionIndex = (binding.optionIndex + 1) % binding.options.size();
        }
        else if (activate)
        {
            auto& value = binding.options[binding.optionIndex].selected.get();
            value = !value;
        }
        break;
    }
    case ItemType::ColorPicker:
    {
        auto& binding = std::get<ColorBinding>(item.binding);
        if (decrease)
        {
            binding.channel = (binding.channel + 2) % 3;
        }
        else if (increase)
        {
            binding.channel = (binding.channel + 1) % 3;
        }
        else if (activate)
        {
            editingColor_ = true;
        }
        break;
    }
    case ItemType::Label:
    case ItemType::Spacer:
        break;
    }
}

void Menu::ProcessKeyCapture(const KeyState& pressed)
{
    if (IsPressed(pressed, VK_ESCAPE))
    {
        capturingKey_ = false;
        return;
    }

    auto& virtualKey = std::get<KeybindBinding>(items_[currentIndex_].binding).virtualKey.get();
    if (IsPressed(pressed, VK_BACK) || IsPressed(pressed, VK_DELETE))
    {
        virtualKey = 0;
        capturingKey_ = false;
        return;
    }

    for (std::size_t key = 1; key < pressed.size(); ++key)
    {
        if (!pressed[key] || key == VK_INSERT || key == VK_RETURN)
        {
            continue;
        }

        virtualKey = static_cast<int>(key);
        capturingKey_ = false;
        return;
    }
}

void Menu::ProcessColorEditor(const KeyState& pressed)
{
    if (IsPressed(pressed, VK_RETURN) || IsPressed(pressed, VK_ESCAPE))
    {
        editingColor_ = false;
        return;
    }

    auto& binding = std::get<ColorBinding>(items_[currentIndex_].binding);
    if (IsPressed(pressed, VK_LEFT))
    {
        binding.channel = (binding.channel + 2) % 3;
    }
    else if (IsPressed(pressed, VK_RIGHT))
    {
        binding.channel = (binding.channel + 1) % 3;
    }

    int direction = 0;
    if (IsPressed(pressed, VK_UP))
    {
        direction = 1;
    }
    else if (IsPressed(pressed, VK_DOWN))
    {
        direction = -1;
    }

    if (direction == 0)
    {
        return;
    }

    Color& color = binding.color.get();
    switch (binding.channel)
    {
    case 0:
        color.r = AdjustChannel(color.r, direction);
        break;
    case 1:
        color.g = AdjustChannel(color.g, direction);
        break;
    case 2:
        color.b = AdjustChannel(color.b, direction);
        break;
    default:
        break;
    }
}

void Menu::RenderItem(
    Renderer& renderer,
    ID3DXFont* font,
    const Item& item,
    std::size_t index,
    Point position) const
{
    const bool selected = IsSelectable(index) && index == currentIndex_;
    const Point rowEnd{position.x + kMenuWidth, position.y + kRowHeight};

    if (selected)
    {
        renderer.RenderFilledRect(position, rowEnd, kSelectedColor);
        renderer.RenderFilledRect(position, {position.x + 3, rowEnd.y}, kAccentColor);
    }

    if (item.type == ItemType::Spacer)
    {
        return;
    }

    if (item.type == ItemType::Label)
    {
        DrawOutlinedText(
            renderer,
            font,
            item.name,
            {position.x, position.y + 3},
            kAccentColor,
            true,
            rowEnd);
        return;
    }

    DrawOutlinedText(
        renderer,
        font,
        item.name,
        {position.x + kPadding, position.y + 3},
        selected ? kTextColor : kMutedColor);

    std::string valueText;
    Color valueColor = selected ? kTextColor : kMutedColor;

    switch (item.type)
    {
    case ItemType::Toggle:
    {
        const bool value = std::get<ToggleBinding>(item.binding).value.get();
        valueText = value ? "ON" : "OFF";
        valueColor = value ? kEnabledColor : kDisabledColor;
        break;
    }
    case ItemType::IntSlider:
        valueText = std::to_string(std::get<IntSliderBinding>(item.binding).value.get());
        break;
    case ItemType::FloatSlider:
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << std::get<FloatSliderBinding>(item.binding).value.get();
        valueText = stream.str();
        break;
    }
    case ItemType::Keybind:
        valueText = capturingKey_ && selected
                        ? "[press a key]"
                        : VirtualKeyName(
                              std::get<KeybindBinding>(item.binding).virtualKey.get());
        break;
    case ItemType::MultiSelect:
    {
        const auto& binding = std::get<MultiSelectBinding>(item.binding);
        const MenuOption& option = binding.options[binding.optionIndex];
        valueText = std::string(option.selected.get() ? "[x] " : "[ ] ") + option.name;
        valueColor = option.selected.get() ? kEnabledColor : valueColor;
        break;
    }
    case ItemType::ColorPicker:
    {
        const auto& binding = std::get<ColorBinding>(item.binding);
        const Color color = binding.color.get();
        constexpr char channels[] = {'R', 'G', 'B'};
        const std::uint8_t values[] = {color.r, color.g, color.b};
        valueText = std::string(editingColor_ && selected ? "< " : "") +
                    channels[binding.channel] + " " +
                    std::to_string(values[binding.channel]) +
                    (editingColor_ && selected ? " >" : "");
        valueColor = color;
        break;
    }
    case ItemType::Label:
    case ItemType::Spacer:
        return;
    }

    const int valueX = position.x + kMenuWidth - kPadding - TextWidth(renderer, font, valueText);
    DrawOutlinedText(renderer, font, valueText, {valueX, position.y + 3}, valueColor);
}
