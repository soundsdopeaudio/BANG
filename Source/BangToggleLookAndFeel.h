#pragma once
#include <JuceHeader.h>

// BangToggleLookAndFeel: draws ALL juce::ToggleButton components using
// your ON/OFF images with hover and down states.
class BangToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BangToggleLookAndFeel()
    {
        // Load all 6 images once at startup. Fall back gracefully if any optional
        // *_hover or *_down images are missing (still works with base images).
        onNormal = loadImage(BinaryData::toggleBtnOn_png, BinaryData::toggleBtnOn_pngSize);
        onOver = tryLoad(BinaryData::toggleBtnOn_hover_png, BinaryData::toggleBtnOn_hover_pngSize, onNormal);
        onDown = tryLoad(BinaryData::toggleBtnOn_down_png, BinaryData::toggleBtnOn_down_pngSize, onOver);

        offNormal = loadImage(BinaryData::toggleBtnOff_png, BinaryData::toggleBtnOff_pngSize);
        offOver = tryLoad(BinaryData::toggleBtnOff_hover_png, BinaryData::toggleBtnOff_hover_pngSize, offNormal);
        offDown = tryLoad(BinaryData::toggleBtnOff_down_png, BinaryData::toggleBtnOff_down_pngSize, offOver);

        // Suggest a default button height based on the "on" art if available
        defaultButtonHeight = (onNormal.isValid() ? onNormal.getHeight() : offNormal.getHeight());
        if (defaultButtonHeight <= 0) defaultButtonHeight = 64; // safe fallback
    }

    // Utility: call this on any ToggleButton you want sized to the artwork width:height ratio
    void sizeToggleToArt(juce::ToggleButton& b, int height = 0) const
    {
        if (height <= 0) height = defaultButtonHeight;
        const int wOn = onNormal.isValid() ? onNormal.getWidth() : 0;
        const int wOff = offNormal.isValid() ? offNormal.getWidth() : 0;
        const int hOn = onNormal.isValid() ? onNormal.getHeight() : 0;
        const int hOff = offNormal.isValid() ? offNormal.getHeight() : 0;

        const int artW = (wOn > 0 ? wOn : wOff);
        const int artH = (hOn > 0 ? hOn : hOff);
        if (artW > 0 && artH > 0)
        {
            const float ratio = (float)artW / (float)artH;
            b.setSize(juce::roundToInt(ratio * height), height);
        }
        else
        {
            b.setSize(180, height > 0 ? height : 64);
        }
    }

    // The money: draw ToggleButton using our 6 images
    void drawToggleButton(juce::Graphics& g,
        juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override
    {
        // Pick the correct image based on toggle + hover/down state
        const bool isOn = button.getToggleState();

        const juce::Image& img =
            isOn
            ? (shouldDrawButtonAsDown ? onDown
                : shouldDrawButtonAsHighlighted ? onOver
                : onNormal)
            : (shouldDrawButtonAsDown ? offDown
                : shouldDrawButtonAsHighlighted ? offOver
                : offNormal);

        // Fill transparent background so the glow/rounded art stands out
        g.fillAll(juce::Colours::transparentBlack);

        const auto bounds = button.getLocalBounds();
        if (img.isValid())
        {
            // Draw the image fitted and centred
            g.drawImageWithin(img, bounds.getX(), bounds.getY(),
                bounds.getWidth(), bounds.getHeight(),
                juce::RectanglePlacement::centred | juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            // Fallback: draw a simple rounded rect if images missing
            auto base = isOn ? juce::Colours::limegreen : juce::Colours::red;
            g.setColour(base.withAlpha(shouldDrawButtonAsDown ? 1.0f : 0.9f));
            g.fillRoundedRectangle(bounds.toFloat().reduced(2.0f), 10.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawText(isOn ? "ON" : "OFF", bounds, juce::Justification::centred, true);
        }
    }

    int getToggleArtWidthForHeight(int h) const
    {
        static int baseW = 0, baseH = 0;
        if (baseW == 0 || baseH == 0)
        {
            juce::MemoryInputStream mis(BinaryData::toggleBtnOn_png,
                BinaryData::toggleBtnOn_pngSize, false);
            if (auto img = juce::ImageFileFormat::loadFrom(mis); img.isValid())
            {
                baseW = img.getWidth(); baseH = img.getHeight();
            }
            else { baseW = 180; baseH = 64; } // safe fallback
        }
        return juce::roundToInt((baseW / (float)baseH) * (float)h);
    }

private:
    static juce::Image loadImage(const void* data, size_t size)
    {
        if (data == nullptr || size == 0) return {};
        juce::MemoryInputStream mis(data, size, false);
        return juce::ImageFileFormat::loadFrom(mis);
    }

    static juce::Image tryLoad(const void* data, size_t size, const juce::Image& fallback)
    {
        auto img = loadImage(data, size);
        return img.isValid() ? img : fallback;
    }

    juce::Image onNormal, onOver, onDown;
    juce::Image offNormal, offOver, offDown;
    int defaultButtonHeight = 64;
};
