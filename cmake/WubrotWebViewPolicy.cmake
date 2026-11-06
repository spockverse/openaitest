# JUCE 8.0.6 has no public WebView2 context-menu option. Apply this narrow,
# idempotent policy to the pinned JUCE sources compiled into WUBROT only.
# No host HWND hooks, accelerator-key restrictions or global browser settings.
function(wubrot_disable_webview_context_menus juce_root)
    set(browser_source "${juce_root}/modules/juce_gui_extra/native/juce_WebBrowserComponent_windows.cpp")
    file(READ "${browser_source}" browser_code)
    set(marker "// WUBROT: suppress native browser menus in this WebView2 instance.")
    string(FIND "${browser_code}" "${marker}" already_patched)
    if(already_patched EQUAL -1)
        set(anchor "            settings->put_IsStatusBarEnabled (! preferences.getWinWebView2BackendOptions().getIsStatusBarDisabled());")
        string(FIND "${browser_code}" "${anchor}" anchor_position)
        if(anchor_position EQUAL -1)
            message(FATAL_ERROR "WUBROT WebView2 policy needs review for this JUCE revision: settings anchor missing")
        endif()
        string(REPLACE "${anchor}"
            "            ${marker}\n            settings->put_AreDefaultContextMenusEnabled (false);\n${anchor}"
            browser_code "${browser_code}")
        file(WRITE "${browser_source}" "${browser_code}")
    endif()
endfunction()
