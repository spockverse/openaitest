# WUBROT development contract

- Current deliverable: portable Windows x64 VST3 ZIPs, built by GitHub Actions. No automatic installers, public releases, macOS or Linux release work.
- Keep installed bundle filename and PRODUCT_NAME `WUBROT.vst3` / `WUBROT`, BUNDLE_ID `audio.spock.wubrot`, manufacturer code `Spck`, plugin code `Wbrt`, parameter IDs and `WubrotState` format stable. Do not break saved FL Studio projects when updating code. New state properties must be optional for older projects.
- Approved development updates get uniquely versioned Actions artifacts; do not version the installed bundle filename. Previous artifacts are retained for 90 days; do not claim permanent archival.
- Embed all UI artwork/scripts in the plugin. Portable content belongs inside Contents/Resources/WubrotContent. No separate UI installation.
- Preserve the approved visuals and DSP unless the user requests changes. UI sizes are Small 600x520, Medium 804x697 (default), Large 996x864. Disable arbitrary resizing; request actual native editor bounds, persist the size with project state, and scale from a 1200x1040 design canvas. The extra height reserves the footer and bottom border; preserve the scale of the main controls.
- Report browser, processor, real-VST3 and real-DAW checks separately. Do not claim FL Studio UI validation without actually testing it.

- User-provided bass recordings are temporary analysis references only. Never commit, embed, bundle, or distribute them or their processed/converted derivatives, including audition ZIPs. Distributed audio tests must be synthetic or explicitly licensed for redistribution. Keep quantitative analysis findings without recordings.

- ROT DNA regionStart/regionEnd/dnaSpeed are appended automatable parameters. New user projects embed their loaded WAV for offline editing (20MB/first30s); this is project data, never factory/build content. Keep reference recordings excluded from all distributed artifacts.
