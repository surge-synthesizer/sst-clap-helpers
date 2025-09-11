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

#ifndef INC_CH_SST_CLAP_JUCE_SHIM_MENU_HELPER_H
#define INC_CH_SST_CLAP_JUCE_SHIM_MENU_HELPER_H

#include <clap/clap.h>

static_assert(__cplusplus >= 202002L, "Surge team libraries have moved to C++ 20");

namespace juce
{
class PopupMenu;
}
namespace sst::clap_juce_shim
{
// If there's a host extension for menus, add a separator then the menus
void populateMenuForClapParam(juce::PopupMenu &, clap_id paramId, const clap_host_t *host);
}; // namespace sst::clap_juce_shim

#endif // MENU_HELPER_H
