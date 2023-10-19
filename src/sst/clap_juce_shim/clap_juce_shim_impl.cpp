#include <iostream>
#include "sst/clap_juce_shim/clap_juce_shim.h"

#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#include <juce_gui_basics/juce_gui_basics.h>


#if JUCE_LINUX
#include <juce_audio_plugin_client/detail/juce_LinuxMessageThread.h>
#endif

#include <memory>

namespace sst::clap_juce_shim
{
namespace details
{
struct Implementor
{
    struct ImplParent : juce::Component
    {
        void paint(juce::Graphics &g) override
        {
            g.fillAll(juce::Colours::black);
        }

        void resized() override
        {
            jassert(getNumChildComponents() <= 1);
            if (getNumChildComponents() == 1)
            {
                getChildComponent(0)->setBounds(getLocalBounds());
            }
        }

    };

    bool guiParentAttached{false};
    void guaranteeSetup()
    {
        if (!guiInitializer)
        {
            guiInitializer = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        }
    }

    void setContents(std::unique_ptr<juce::Component> &c)
    {
        jassert(!editor);
        jassert(!implParent);
        editor = std::move(c);
        implParent = std::make_unique<ImplParent>();
        implParent->addAndMakeVisible(*editor);
        implParent->setSize(editor->getWidth(), editor->getHeight());
    }

    void destroy()
    {
        if (guiParentAttached && implParent && editor)
        {
            implParent->removeAllChildren();
            editor.reset(nullptr);
            implParent.reset(nullptr);
        }
    }

    juce::Component *comp() { return implParent.get(); }

    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> guiInitializer{nullptr}; // todo deal with lifecycle

  protected:
    std::unique_ptr<ImplParent> implParent{nullptr};
    std::unique_ptr<juce::Component> editor{nullptr};
};
} // namespace details
// #define TRACE std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
#define TRACE ;
ClapJuceShim::ClapJuceShim(EditorProvider *ep)
    : editorProvider(ep)
{
    impl = std::make_unique<details::Implementor>();
}

ClapJuceShim::~ClapJuceShim() {}

bool ClapJuceShim::isEditorAttached() { return impl->guiParentAttached; }
bool ClapJuceShim::guiAdjustSize(uint32_t *w, uint32_t *h) noexcept { return true; }

bool ClapJuceShim::guiSetSize(uint32_t width, uint32_t height) noexcept
{
    TRACE;

    auto uw = static_cast<int32_t>(width), uh = static_cast<int32_t>(height);
    // SCALE SUPPORT - do we need to transform here?
    // impl->comp()->getTransform().transformPoint(uw, uh);
    impl->comp()->setSize(uw, uh);
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
    editorProvider->registerOrUnregisterTimer(idleTimerId, 1000/50, true);
#endif

    impl->guiInitializer = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
    juce::ignoreUnused(api);

    // Should never happen
    if (isFloating)
        return false;

    const juce::MessageManagerLock mmLock;
    auto ed = editorProvider->createEditor();
    impl->setContents(ed);
    return impl->comp() != nullptr;
}

void ClapJuceShim::guiDestroy() noexcept
{
    TRACE;
#if JUCE_LINUX
    editorProvider->registerOrUnregisterTimer(idleTimerId, 0, false);
#endif

    impl->guiParentAttached = false;
    impl->destroy();
}

bool ClapJuceShim::guiSetParent(const clap_window *window) noexcept
{
    TRACE;
    impl->guiParentAttached = true;
#if JUCE_MAC
    extern bool guiCocoaAttach(const clap_window *, juce::Component *);
    auto res = guiCocoaAttach(window, impl->comp());
    impl->comp()->repaint();
    return res;
#elif JUCE_LINUX
    const juce::MessageManagerLock mmLock;
    impl->comp()->setVisible(false);
    impl->comp()->addToDesktop(0, (void *)window->x11);
    auto *display = juce::XWindowSystem::getInstance()->getDisplay();
    juce::X11Symbols::getInstance()->xReparentWindow(
        display, (Window)impl->comp()->getWindowHandle(), window->x11, 0, 0);
    impl->comp()->setVisible(true);
    return true;

    return false;
#elif JUCE_WINDOWS
    impl->comp()->setVisible(false);
    impl->comp()->setOpaque(true);
    impl->comp()->setTopLeftPosition(0, 0);
    impl->comp()->addToDesktop(0, (void *)window->win32);
    impl->comp()->setVisible(true);
    return true;
#else
    impl->guiParentAttached = false;
    return false;
#endif
}

// Show doesn't really exist in JUCE per se. If there's an impl->comp() and its attached
// we are good.
bool ClapJuceShim::guiShow() noexcept
{
    TRACE;
#if JUCE_MAC || JUCE_LINUX || JUCE_WINDOWS
    if (impl->comp())
    {
        return impl->guiParentAttached;
    }
#endif
    return false;
}

bool ClapJuceShim::guiGetSize(uint32_t *width, uint32_t *height) noexcept
{
    TRACE;
    const juce::MessageManagerLock mmLock;
    if (impl->comp())
    {
        auto b = impl->comp()->getBounds();
        *width = (uint32_t)b.getWidth();
        *height = (uint32_t)b.getHeight();
        return true;
    }
    else
    {
        *width = 1000;
        *height = 800;
    }
    return false;
}

bool ClapJuceShim::guiSetScale(double scale) noexcept {
    TRACE;
    // SCALE SUPPORT 
    // If you want to start supporting HDPI we turn this on and then make sure all the sizes match up
    // impl->comp()->setTransform(juce::AffineTransform().scaled(scale));
    return true;
}

#if JUCE_LINUX
void ClapJuceShim::onTimer(clap_id timerId) noexcept {
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
