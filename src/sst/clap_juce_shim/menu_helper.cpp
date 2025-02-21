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

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include "sst/clap_juce_shim/menu_helper.h"

#include "clap/ext/context-menu.h"
#include <deque>

namespace sst::clap_juce_shim
{

struct mhCtx
{
    std::deque<juce::PopupMenu *> menus;
    std::deque<std::string> menuNames;
    const clap_host_t *host{nullptr};
    clap_id parId{0};
    bool firstTime{true};
    mhCtx(juce::PopupMenu &p) { menus.push_back(&p); };
};

const clap_host_context_menu_t *mhGetExt(const clap_host_t *host)
{
    auto ext = host->get_extension(host, CLAP_EXT_CONTEXT_MENU);
    if (!ext)
    {
        ext = host->get_extension(host, CLAP_EXT_CONTEXT_MENU_COMPAT);
        if (!ext)
        {
            return nullptr;
        }
    }
    return static_cast<const clap_host_context_menu_t *>(ext);
}

bool mhAddItem(const struct clap_context_menu_builder *builder,
               clap_context_menu_item_kind_t item_kind, const void *item_data)
{
    auto mhc = static_cast<mhCtx *>(builder->ctx);
    if (mhc->firstTime)
    {
        mhc->menus.back()->addSeparator();
        mhc->firstTime = false;
    }
    auto &currm = mhc->menus.back();
    switch (item_kind)
    {
    case CLAP_CONTEXT_MENU_ITEM_ENTRY:
    {
        auto it = static_cast<const clap_context_menu_entry_t *>(item_data);
        currm->addItem(it->label, it->is_enabled, false,
                       [paramId = mhc->parId, host = mhc->host, id = it->action_id]() {
                           auto ext = mhGetExt(host);
                           if (ext)
                           {
                               clap_context_menu_target t;
                               t.kind = CLAP_CONTEXT_MENU_TARGET_KIND_PARAM;
                               t.id = paramId;
                               ext->perform(host, &t, id);
                           }
                       });
    }
    break;
    case CLAP_CONTEXT_MENU_ITEM_CHECK_ENTRY:
    {
        auto it = static_cast<const clap_context_menu_check_entry_t *>(item_data);
        currm->addItem(juce::CharPointer_UTF8(it->label), it->is_enabled, it->is_checked,
                       [paramId = mhc->parId, host = mhc->host, id = it->action_id]() {
                           auto ext = mhGetExt(host);
                           if (ext)
                           {
                               clap_context_menu_target t;
                               t.kind = CLAP_CONTEXT_MENU_TARGET_KIND_PARAM;
                               t.id = paramId;
                               ext->perform(host, &t, id);
                           }
                       });
    }
    break;
    case CLAP_CONTEXT_MENU_ITEM_SEPARATOR:
    {

        currm->addSeparator();
    }
    break;
    case CLAP_CONTEXT_MENU_ITEM_BEGIN_SUBMENU:
    {
        auto it = static_cast<const clap_context_menu_submenu_t *>(item_data);
        mhc->menuNames.push_back(it->label);
        mhc->menus.push_back(new juce::PopupMenu());
    }
    break;
    case CLAP_CONTEXT_MENU_ITEM_END_SUBMENU:
    {
        auto mn = mhc->menus.back();
        auto mnm = mhc->menuNames.back();

        mhc->menus.pop_back();
        mhc->menuNames.pop_back();

        mhc->menus.back()->addSubMenu(mnm, *mn);
        delete mn;
    }
    break;

    case CLAP_CONTEXT_MENU_ITEM_TITLE:
    {
        auto it = static_cast<const clap_context_menu_item_title_t *>(item_data);

        currm->addSectionHeader(it->title); // , it->is_enabled);
    }
    break;
    }
    return true;
}

// Returns true if the menu builder supports the given item kind
bool mhSupports(const struct clap_context_menu_builder *builder,
                clap_context_menu_item_kind_t item_kind)
{
    return true;
}

// If there's a host extension for menus, add a separator then the menus
void populateMenuForClapParam(juce::PopupMenu &p, clap_id paramId, const clap_host_t *host)
{
    if (!host)
        return;

    auto ext = mhGetExt(host);
    if (!ext)
    {
        return;
    }

    auto chcm = static_cast<const clap_host_context_menu_t *>(ext);

    auto mhc = mhCtx(p);
    mhc.host = host;
    mhc.parId = paramId;

    clap_context_menu_target t;
    t.kind = CLAP_CONTEXT_MENU_TARGET_KIND_PARAM;
    t.id = paramId;

    clap_context_menu_builder b;
    b.ctx = &mhc;
    b.add_item = mhAddItem;
    b.supports = mhSupports;

    chcm->populate(host, &t, &b);

    return;
}
} // namespace sst::clap_juce_shim