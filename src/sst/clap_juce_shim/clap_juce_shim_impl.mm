//
// Created by Paul Walker on 9/2/23.
//

#include <Cocoa/Cocoa.h>
#include <juce_core/juce_core.h>
#include "clap/ext/gui.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace sst::clap_juce_shim
{
bool guiCocoaAttach(const clap_window_t *window, juce::Component *comp)
{
    auto nsv = (NSView *)window->cocoa;
    @autoreleasepool
    {
        const auto defaultFlags = 0;
        comp->addToDesktop(defaultFlags, nsv);
        comp->setVisible(true);
        comp->toFront(false);

        [[nsv window] setAcceptsMouseMovedEvents:YES];
    }
    return true;
}

bool guiCocoaDetach(const clap_window_t *window) { return true; }
} // namespace sst::clap_juce_shim