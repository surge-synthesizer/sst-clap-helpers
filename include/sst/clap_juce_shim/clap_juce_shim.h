/*
 * sst-clap_helpers - an open source library of stuff which makes
 * making clap easier for the Surge Synth Team.
 *
 * Copyright 2023-2025, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-clap-helpers is released under the MIT license, as described
 * by "LICENSE.md" in this repository.
 *
 * All source in sst-jucegui available at
 * https://github.com/surge-synthesizer/sst-clap-helpers
 */

#include <functional>
#include <memory>
#include <cstdint>

#include "clap/ext/gui.h"

#ifndef INC_CH_SST_CLAP_JUCE_SHIM_CLAP_JUCE_SHIM_H
#define INC_CH_SST_CLAP_JUCE_SHIM_CLAP_JUCE_SHIM_H

namespace juce
{
class Component;
}
namespace sst::clap_juce_shim
{
namespace details
{
struct Implementor;
}

struct EditorProvider
{
    virtual ~EditorProvider() = default;
    virtual std::unique_ptr<juce::Component> createEditor() = 0;
    virtual bool registerOrUnregisterTimer(clap_id &, int, bool) = 0;
};
struct ClapJuceShim
{
    ClapJuceShim(EditorProvider *editorProvider);
    ~ClapJuceShim();

    void setResizable(bool b) { resizable = b; }

    std::unique_ptr<details::Implementor> impl;
    bool resizable{false};

    bool isEditorAttached();

    bool guiCanResize() const noexcept { return resizable; }
    bool guiIsApiSupported(const char *api, bool isFloating) noexcept;

    bool guiCreate(const char *api, bool isFloating) noexcept;
    void guiDestroy() noexcept;
    bool guiSetParent(const clap_window *window) noexcept;

    bool guiSetScale(double scale) noexcept;
    bool guiAdjustSize(uint32_t *width, uint32_t *height) noexcept;
    bool guiSetSize(uint32_t width, uint32_t height) noexcept;
    bool guiGetSize(uint32_t *width, uint32_t *height) noexcept;

    bool guiShow() noexcept;

#if SHIM_LINUX
    clap_id idleTimerId{0};
    void onTimer(clap_id timerId) noexcept;

#endif

    EditorProvider *editorProvider;

    double getGuiScale() const
    {
#if SHIM_WINDOWS || SHIM_LINUX
        return guiScale;
#else
        return 1.0; // mac is unscaled
#endif
    }

  private:
    double guiScale{1.0};
    void dumpSizeDebugInfo(const std::string &pfx, const std::string &func, int line);
};
} // namespace sst::clap_juce_shim

#define ADD_SHIM_IMPLEMENTATION(clapJuceShim)                                                      \
    bool guiCanResize() const noexcept override { return clapJuceShim->guiCanResize(); }           \
    bool guiIsApiSupported(const char *api, bool isFloating) noexcept override                     \
    {                                                                                              \
        return clapJuceShim->guiIsApiSupported(api, isFloating);                                   \
    }                                                                                              \
    bool guiCreate(const char *api, bool isFloating) noexcept override                             \
    {                                                                                              \
        return clapJuceShim->guiCreate(api, isFloating);                                           \
    }                                                                                              \
    void guiDestroy() noexcept override { clapJuceShim->guiDestroy(); }                            \
    bool guiSetParent(const clap_window *window) noexcept override                                 \
    {                                                                                              \
        return clapJuceShim->guiSetParent(window);                                                 \
    }                                                                                              \
    bool guiSetScale(double scale) noexcept override { return clapJuceShim->guiSetScale(scale); }  \
    bool guiAdjustSize(uint32_t *width, uint32_t *height) noexcept override                        \
    {                                                                                              \
        return clapJuceShim->guiAdjustSize(width, height);                                         \
    }                                                                                              \
    bool guiSetSize(uint32_t width, uint32_t height) noexcept override                             \
    {                                                                                              \
        return clapJuceShim->guiSetSize(width, height);                                            \
    }                                                                                              \
    bool guiGetSize(uint32_t *width, uint32_t *height) noexcept override                           \
    {                                                                                              \
        return clapJuceShim->guiGetSize(width, height);                                            \
    }                                                                                              \
    bool guiShow() noexcept override { return clapJuceShim->guiShow() && onShow(); }               \
    std::function<bool(void)> onShow{[]() { return true; }};

#if SHIM_LINUX
#define ADD_SHIM_LINUX_TIMER(clapJuceShim)                                                         \
    bool implementsTimerSupport() const noexcept override { return true; }                         \
    void onTimer(clap_id timerId) noexcept override { clapJuceShim->onTimer(timerId); }
#else
#define ADD_SHIM_LINUX_TIMER(clapJuceShim)
#endif

#endif // CLAP_SAW_JUICY_CLAP_JUCE_SHIM_H
