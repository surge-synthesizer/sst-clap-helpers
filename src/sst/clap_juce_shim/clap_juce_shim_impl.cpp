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
#include <iostream>
#include "sst/clap_juce_shim/clap_juce_shim.h"

#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_LINUX
#include <juce_audio_plugin_client/detail/juce_LinuxMessageThread.h>
#endif

#include <memory>

#if JUCE_WINDOWS
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>
#endif

#define FLF __FILE__ << ":" << __LINE__ << " " << __func__ << " "

#define DO_TRACE 0
#if DO_TRACE
#define TRACE std::cout << FLF << std::endl;
#define TRACEOUT(x) std::cout << FLF << x << std::endl;
#define SZTRACE(x) dumpSizeDebugInfo(x, __func__, __LINE__);
#else
#define TRACE ;
#define TRACEOUT(x) ;
#define SZTRACE(x) ;
#endif

namespace sst::clap_juce_shim
{
namespace details
{
struct Implementor
{
#if JUCE_WINDOWS
    juce::detail::WindowsHooks hooks;
#endif

    struct ImplParent : juce::Component
    {
        std::string displayName;
        bool rescaleChild;
        ImplParent(const std::string &nm, bool rsc) : displayName(nm), rescaleChild(rsc)
        {
            TRACEOUT("Creating an impl parent " << displayName);
            setAccessible(true);
            setTitle("Implementation Parent " + displayName);
        }

        ~ImplParent() { TRACEOUT("DESTROYING an impl parent"); }

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
        {
            return std::make_unique<juce::AccessibilityHandler>(*this,
                                                                juce::AccessibilityRole::ignored);
        }

        void paint(juce::Graphics &g) override { g.fillAll(juce::Colours::black); }

        void resized() override
        {
            jassert(getNumChildComponents() <= 1);

            if (getNumChildComponents() == 1)
            {
                auto w = getLocalBounds().getWidth();
                auto h = getLocalBounds().getHeight();

                TRACEOUT("pre - w/h = " << w << " " << h << " " << displayName << " "
                                        << rescaleChild);

                // As we go downwards we unwind the transform
                getTransform().inverted().transformPoint(w, h);

                TRACEOUT("post - w/h = " << w << " " << h << " " << displayName << " "
                                         << rescaleChild);
                TRACEOUT(getChildComponent(0)->getTitle()
                         << " xf=" << getChildComponent(0)->getTransform().getScaleFactor())
                if (rescaleChild)
                    getChildComponent(0)->getTransform().inverted().transformPoint(w, h);
                TRACEOUT("postpost - w/h = " << w << " " << h << " " << displayName << " "
                                             << rescaleChild);

                getChildComponent(0)->setBounds(0, 0, w, h);
            }
        }

        void visibilityChanged() override { TRACE; }
        void parentHierarchyChanged() override { TRACE; }
    };

    const clap_window *guiParentWindow{nullptr};
    bool guiParentAttached{false};
    void guaranteeSetup()
    {
        TRACE;
        if (!guiInitializer)
        {
            guiInitializer = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        }
    }

    void setContents(std::unique_ptr<juce::Component> &c)
    {
        jassert(!editor);
        jassert(!implDesktop);
        editor = std::move(c);
        implDesktop = std::make_unique<ImplParent>("Desktop", false);
        implHolder = std::make_unique<ImplParent>("Holder", true);
        implDesktop->addAndMakeVisible(*implHolder);
        implHolder->addAndMakeVisible(*editor);
        implHolder->setSize(editor->getWidth(), editor->getHeight());
        implDesktop->setSize(editor->getWidth(), editor->getHeight());
    }

    void destroy()
    {
        TRACE;
        if (guiParentAttached && implDesktop && editor)
        {
            guiParentAttached = false;
            implDesktop->removeAllChildren();
            editor.reset(nullptr);
            implHolder.reset(nullptr);
            implDesktop.reset(nullptr);
        }
    }

    juce::Component *desktop() { return implDesktop.get(); }
    juce::Component *edHolder() { return implHolder.get(); }
    juce::Component *ed() { return editor.get(); }

    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> guiInitializer{
        nullptr}; // todo deal with lifecycle

  protected:
    std::unique_ptr<ImplParent> implDesktop{nullptr}, implHolder{nullptr};
    std::unique_ptr<juce::Component> editor{nullptr};
};
} // namespace details

ClapJuceShim::ClapJuceShim(EditorProvider *ep) : editorProvider(ep)
{
    impl = std::make_unique<details::Implementor>();
}

ClapJuceShim::~ClapJuceShim() {}

bool ClapJuceShim::isEditorAttached() { return impl->guiParentAttached; }
bool ClapJuceShim::guiAdjustSize(uint32_t *w, uint32_t *h) noexcept { return true; }

bool ClapJuceShim::guiSetSize(uint32_t width, uint32_t height) noexcept
{
    TRACEOUT(" W=" << width << " H=" << height)

    SZTRACE("Pre guiSetSize");
    auto uw = static_cast<int32_t>(width), uh = static_cast<int32_t>(height);
    impl->desktop()->setSize(uw, uh);

    SZTRACE("Post guiSetSize");
    return true;
}

bool ClapJuceShim::guiIsApiSupported(const char *api, bool isFloating) noexcept
{
    TRACE;
    if (isFloating)
        return false;

    if (strcmp(api, CLAP_WINDOW_API_WIN32) == 0 || strcmp(api, CLAP_WINDOW_API_COCOA) == 0 ||
        strcmp(api, CLAP_WINDOW_API_X11) == 0)
        return true;

    return false;
}

bool ClapJuceShim::guiCreate(const char *api, bool isFloating) noexcept
{
    TRACE;
    impl->guaranteeSetup();
#if JUCE_LINUX
    idleTimerId = 0;
    editorProvider->registerOrUnregisterTimer(idleTimerId, 1000 / 50, true);
#endif

    impl->guiInitializer = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    juce::ignoreUnused(api);

    // Should never happen
    if (isFloating)
        return false;

    const juce::MessageManagerLock mmLock;
    auto ed = editorProvider->createEditor();
    impl->setContents(ed);
    return impl->desktop() != nullptr;
}

void ClapJuceShim::guiDestroy() noexcept
{
    TRACE;
#if JUCE_LINUX
    editorProvider->registerOrUnregisterTimer(idleTimerId, 0, false);
#endif

    impl->destroy();
    impl->guiParentAttached = false;

#if JUCE_MAC
    extern bool guiCocoaDetach(const clap_window *);
    auto res = guiCocoaDetach(impl->guiParentWindow);
#endif

    impl->guiParentWindow = nullptr;
}

bool ClapJuceShim::guiSetParent(const clap_window *window) noexcept
{
    TRACE;
    TRACEOUT("Creating : GPA=" << impl->guiParentAttached << " GW=" << impl->guiParentWindow
                               << " W=" << window);

    if (impl->guiParentAttached && window == impl->guiParentWindow)
    {
        /*
         * VCV Rack, through the VST3 wrapper in host, will not call
         * guiDestory on windows close but it will call guiSetParent on the
         * next time through. If that's the case our internal state will be
         * wrong and we won't have removed ourself. So at least remove ourselves
         * so when we reparent to our existing window we show.
         */
        impl->desktop()->removeFromDesktop();
    }
    impl->guiParentAttached = true;
    impl->guiParentWindow = window;
#if JUCE_MAC
    extern bool guiCocoaAttach(const clap_window *, juce::Component *);
    auto res = guiCocoaAttach(window, impl->desktop());
    impl->desktop()->repaint();
    return res;
#elif JUCE_LINUX
    const juce::MessageManagerLock mmLock;
    impl->desktop()->setVisible(false);
    impl->desktop()->addToDesktop(0, (void *)window->x11);
    auto *display = juce::XWindowSystem::getInstance()->getDisplay();
    juce::X11Symbols::getInstance()->xReparentWindow(
        display, (Window)impl->desktop()->getWindowHandle(), window->x11, 0, 0);
    impl->desktop()->setVisible(true);
    return true;

#elif JUCE_WINDOWS
    impl->desktop()->setVisible(false);
    impl->desktop()->setOpaque(true);
    impl->desktop()->setTopLeftPosition(0, 0);
    impl->desktop()->addToDesktop(0, (void *)window->win32);
    impl->desktop()->setVisible(true);
    return true;
#else
    impl->guiParentAttached = false;
    return false;
#endif
}

// Show doesn't really exist in JUCE per se. If there's an impl->desktop() and its attached
// we are good.
bool ClapJuceShim::guiShow() noexcept
{
    TRACE;
#if JUCE_MAC || JUCE_LINUX || JUCE_WINDOWS
    if (impl->desktop())
    {
        SZTRACE("Size at show");
        return impl->guiParentAttached;
    }
#endif
    return false;
}

bool ClapJuceShim::guiGetSize(uint32_t *width, uint32_t *height) noexcept
{
    TRACE;
    const juce::MessageManagerLock mmLock;
    if (impl->desktop())
    {
        auto b = impl->edHolder()->getBounds();

        *width = (uint32_t)b.getWidth();
        *height = (uint32_t)b.getHeight();

        TRACEOUT(" W=" << *width << " H=" << *height);

        return true;
    }
    else
    {
        *width = 1000;
        *height = 800;
    }
    return false;
}

bool ClapJuceShim::guiSetScale(double scale) noexcept
{
    TRACEOUT(" scale=" << scale);

#if JUCE_LINUX
    return false;
    // This should in theory work on linux now. Need to test it tho
    // impl->edHolder()->setTransform(juce::AffineTransform().scaled(scale));
    // impl->desktop()->setBounds(impl->edHolder()->getBoundsInParent());
#else

#if JUCE_WINDOWS
    impl->edHolder()->setTransform(juce::AffineTransform().scaled(scale));
    impl->edHolder()->resized();
    impl->desktop()->setBounds(impl->edHolder()->getBoundsInParent());
#endif
    guiScale = scale;
    return true;
#endif
}

void ClapJuceShim::dumpSizeDebugInfo(const std::string &pfx, const std::string &func, int line)
{
    auto sf = [](const auto &tf) { return std::sqrt(std::abs(tf.getDeterminant())); };
    std::cout << __FILE__ << ":" << line << " " << func << " " << pfx << "\n"
              << "   guiScale=" << guiScale << "\n   Desk=("
              << impl->desktop()->getBounds().toString()
              << " / xf = " << sf(impl->desktop()->getTransform()) << ")\n   Hold=("
              << impl->edHolder()->getBounds().toString()
              << " / xf = " << sf(impl->edHolder()->getTransform()) << ")\n   HoldInParent=("
              << impl->edHolder()->getBoundsInParent().toString() << ")\n   Ed=("
              << impl->ed()->getBounds().toString() << " / xf = " << sf(impl->ed()->getTransform())
              << ")\n   EdInParent=(" << impl->ed()->getBoundsInParent().toString() << ")"
              << std::endl;
}
#if JUCE_LINUX
void ClapJuceShim::onTimer(clap_id timerId) noexcept
{
    if (timerId != idleTimerId)
        return;

    juce::ScopedJuceInitialiser_GUI libraryInitialiser;
    const juce::MessageManagerLock mmLock;

    while (juce::detail::dispatchNextMessageOnSystemQueue(true))
    {
    }
}
#endif

} // namespace sst::clap_juce_shim
