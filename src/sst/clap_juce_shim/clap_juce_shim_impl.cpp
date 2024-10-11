#include <iostream>
#include "sst/clap_juce_shim/clap_juce_shim.h"

#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_LINUX
#include <juce_audio_plugin_client/detail/juce_LinuxMessageThread.h>
#endif

#include <memory>

#define FLF __FILE__ << ":" << __LINE__ << " " << __func__ << " "

namespace sst::clap_juce_shim
{
namespace details
{
struct Implementor
{
    struct ImplParent : juce::Component
    {
        ImplParent()
        {
            setAccessible(true);
            setTitle("Implementation Parent");
            setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
            setWantsKeyboardFocus(true);
        }

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
                getChildComponent(0)->getTransform().inverted().transformPoint(w, h);
                getChildComponent(0)->setBounds(0, 0, w, h);
            }
        }
    };

    const clap_window *guiParentWindow{nullptr};
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
        jassert(!implDesktop);
        editor = std::move(c);
        implDesktop = std::make_unique<ImplParent>();
        implHolder = std::make_unique<ImplParent>();
        implDesktop->addAndMakeVisible(*implHolder);
        implHolder->addAndMakeVisible(*editor);
        implHolder->setSize(editor->getWidth(), editor->getHeight());
        implDesktop->setSize(editor->getWidth(), editor->getHeight());
    }

    void destroy()
    {
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

#define DO_TRACE 1
#if DO_TRACE
#define TRACE std::cout << FLF << std::endl;
#define TRACEOUT(x) std::cout << FLF << x << std::endl;
#define SZTRACE(x) dumpSizeDebugInfo(x, __func__, __LINE__);
#else
#define TRACE ;
#define TRACEOUT(x) ;
#define SZTRACE(x) ;
#endif

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
        auto b = impl->edHolder()->getBoundsInParent();
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
    // impl->edHolder()->setTransform(juce::AffineTransform().scaled(scale));
    // impl->desktop()->setBounds(impl->edHolder()->getBoundsInParent());
#else
    guiScale = scale;
    return true;
#endif
}

void ClapJuceShim::dumpSizeDebugInfo(const std::string &pfx, const std::string &func, int line)
{
    auto sf = [](const auto &tf) { return std::sqrt(std::abs(tf.getDeterminant())); };
    std::cout << __FILE__ << ":" << line << " " << func << " " << pfx << " guiScale=" << guiScale
              << " D=(" << impl->desktop()->getBounds().toString()
              << " / xf = " << sf(impl->desktop()->getTransform()) << ") H=("
              << impl->edHolder()->getBounds().toString()
              << " / xf = " << sf(impl->edHolder()->getTransform()) << ") HIP=("
              << impl->edHolder()->getBoundsInParent().toString() << ") E=("
              << impl->ed()->getBounds().toString() << " / xf = " << sf(impl->ed()->getTransform())
              << ")" << std::endl;
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
