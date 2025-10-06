#pragma once
#include <JuceHeader.h>

// Solid-black outlined sliders to match the mockup (outline around track + thumb only).
class BangSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float /*minPos*/, float /*maxPos*/,
        const juce::Slider::SliderStyle /*style*/,
        juce::Slider& slider) override
    {
        const auto trackCol = slider.findColour(juce::Slider::trackColourId);
        const auto thumbCol = slider.findColour(juce::Slider::thumbColourId);
        const auto bgCol = juce::Colours::black.withAlpha(0.18f);

        const int trackThickness = 10;         // height/width of the track "pill"
        const float r = trackThickness * 0.5f; // rounded corners
        const int thumbSize = 12;
        const float outlinePx = 2.0f;

        if (slider.isHorizontal())
        {
            // === TRACK (centered) ===
            juce::Rectangle<float> track((float)x,
                (float)y + (h - trackThickness) * 0.5f,
                (float)w,
                (float)trackThickness);

            // Background
            g.setColour(bgCol);
            g.fillRoundedRectangle(track, r);

            // Filled portion
            const float frac = juce::jlimit(0.0f, 1.0f, (sliderPos - (float)x) / (float)w);
            juce::Rectangle<float> filled = track.withWidth(track.getWidth() * frac);
            g.setColour(trackCol);
            g.fillRoundedRectangle(filled, r);

            // Outline ONLY around the track (not the whole component)
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(track, r, outlinePx);

            // === THUMB ===
            const float tx = sliderPos - thumbSize * 0.5f;
            const float ty = track.getCentreY() - thumbSize * 0.5f;

            g.setColour(thumbCol);
            g.fillEllipse(tx, ty, (float)thumbSize, (float)thumbSize);

            // Black ring around the thumb
            g.setColour(juce::Colours::black);
            g.drawEllipse(tx, ty, (float)thumbSize, (float)thumbSize, outlinePx);
        }
        else // vertical
        {
            // === TRACK (centered) ===
            juce::Rectangle<float> track((float)x + (w - trackThickness) * 0.5f,
                (float)y,
                (float)trackThickness,
                (float)h);

            // Background
            g.setColour(bgCol);
            g.fillRoundedRectangle(track, r);

            // Filled portion (bottom up to thumb)
            const float frac = juce::jlimit(0.0f, 1.0f, ((float)y + (float)h - sliderPos) / (float)h);
            const float filledH = track.getHeight() * frac;
            juce::Rectangle<float> filled = track.withY(track.getBottom() - filledH)
                .withHeight(filledH);
            g.setColour(trackCol);
            g.fillRoundedRectangle(filled, r);

            // Outline ONLY around the track
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(track, r, outlinePx);

            // === THUMB ===
            const float tx = track.getCentreX() - thumbSize * 0.5f;
            const float ty = sliderPos - thumbSize * 0.5f;

            g.setColour(thumbCol);
            g.fillEllipse(tx, ty, (float)thumbSize, (float)thumbSize);

            // Black ring around the thumb
            g.setColour(juce::Colours::black);
            g.drawEllipse(tx, ty, (float)thumbSize, (float)thumbSize, outlinePx);
        }
    }
};