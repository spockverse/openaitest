# WUBROT development contract

- Current deliverable: portable Windows x64 VST3 ZIPs, built by GitHub Actions. No automatic installers, public releases, macOS or Linux release work.
- Keep installed bundle filename and PRODUCT_NAME `WUBROT.vst3` / `WUBROT`, BUNDLE_ID `audio.spock.wubrot`, manufacturer code `Spck`, plugin code `Wbrt`, parameter IDs and `WubrotState` format stable. Do not break saved FL Studio projects when updating code. New state properties must be optional for older projects.
- Approved development updates get uniquely versioned Actions artifacts; do not version the installed bundle filename. Previous artifacts are retained for 90 days; do not claim permanent archival.
- Embed all UI artwork/scripts in the plugin. Portable content belongs inside Contents/Resources/WubrotContent. No separate UI installation.
- Preserve the approved visuals and DSP unless the user requests changes. UI sizes are Small 600x500, Medium 800x666 (default), Large 1000x833. Disable arbitrary resizing; request actual native editor bounds, persist the size with project state, and scale the whole UI from one fixed design coordinate system.
- Report browser, processor, real-VST3 and real-DAW checks separately. Do not claim FL Studio UI validation without actually testing it.
