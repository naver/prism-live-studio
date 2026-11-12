# PRISM Live Studio
**Learn more about PRISM Live Studio and get the latest updates on our [official website](http://prismlive.com/ko_kr/pcapp/).**

## Versions

- [6.0.3 Beta](#version-603) — Latest
- [6.0.2 Beta](#version-602)
- [6.0.1 Beta](#version-601)
- [6.0.0 Beta](#version-600-beta)

**Before installing**

- B2B support is not yet available in the Beta. B2B customers should continue using the previous version.
- The Beta cannot be installed alongside the existing PRISM Live Studio on the same PC. Installing it will remove and replace the existing version.
- The previous version can be reinstalled from the PRISM website at any time.
- Your existing scenes and settings will be migrated. However, some sources that are no longer supported cannot be migrated and will be removed.
- The existing PRISM Lens app will not be removed, but it cannot be used with the Beta through the previous capture-device connection method. Lens effects, such as face retouching, can be applied to an in-app camera source using the Lens Filter and must be configured again.
- The Beta has a different interface and workflow, so it may take some time to get used to. We recommend installing it when you have enough time rather than shortly before a scheduled broadcast.

<a id="version-603"></a>
## 6.0.3 Beta

### Downloads

- [PRISM Live Studio 6.0.3 Beta for Windows](https://resource-prismlive.pstatic.net/202611121044/PrismLiveStudio_Setup_x64_6.0.3.129.exe)

### New Features

- Added KICK account linking, including channel connection, broadcast title and category settings, and support for single-platform streaming and multistreaming.
- Added KICK support to All Chat, Chat Overlay, and Viewer Count Widget, including chat moderation and subscription and gift subscription messages.

### Changes

- PRISM Live Studio 6.0 Beta now supports NAVER Cloud B2B login and streaming, including service channels, scheduled broadcasts, service browser Docks, notices, output settings guides, chat, and viewer counts.
- Optimized Dual Output encoding to reduce redundant horizontal video encoding when recording or Replay Buffer uses the stream encoder with compatible settings.
- Improved responsiveness during application startup, channel restoration, and scene collection switching.
- Improved responsiveness when preparing YouTube broadcasts and stopping streams.
- Improved Scene Template browsing, detail previews, and thumbnail loading, and added a loading indicator when fetching more templates.
- Reduced resource usage for inactive Lens effects, chat sources, Clock Widgets, GIPHY Stickers, and hidden portrait previews, and optimized Blur Filter and Spout2 processing.
- Optimized static Background Templates to reduce unnecessary rendering when the image content and dimensions remain unchanged.
- Improved loading of NAVER Shopping LIVE schedules, product lists, and images, and reduced cache usage.
- Unified platform ordering across channel lists and menus, and improved compatibility with customized Windows contrast themes.

### Bug Fixes

#### Streaming and Channels

- Fixed incomplete cleanup after some channel startup failures that could affect subsequent attempts.
- Fixed issues with editing broadcast information, stopping a stream, or starting another broadcast after recovering a Facebook Page stream.
- Fixed missing error feedback, unavailable retries, and duplicate authorization-expiry prompts in NAVER Shopping LIVE broadcast information.
- Fixed failures to retrieve NAVER Shopping LIVE broadcast information during recovery potentially preventing the original broadcast from stopping correctly.
- Fixed a channel being unexpectedly deselected when its broadcast was ended by the platform during multistreaming.
- Fixed YouTube scheduled broadcasts not retaining their original automatic start and stop settings after a rehearsal ended.
- Fixed YouTube chat Docks remaining blank or becoming unavailable after disappearing in some situations.
- Fixed the floating All Chat Dock title bar appearing off-screen, preventing the window from being dragged.

#### Dual Output

- Fixed portrait layouts potentially being reset when duplicating scenes containing groups.
- Fixed some scene collection operations not being properly restricted, potentially affecting portrait layouts and output during streaming.
- Fixed new scenes potentially inheriting the portrait layout of a deleted scene with the same name.
- Fixed issues with transition synchronization and scene structure updates.
- Fixed previously ended channels potentially restarting when recovering a stream after an application crash.

#### Sources and Lens

- Fixed Text Template and Viewer Count Widget potentially displaying blank frames when shown again after being hidden.
- Fixed downloaded Text Templates potentially being applied to the wrong source after switching sources.
- Fixed incorrect Text Template dimensions, text, or colors after canvas changes or asynchronous template downloads.
- Fixed Chat Overlay backgrounds selected while offline failing to appear after the network connection was restored.
- Fixed Viewer Count Widget jumping or briefly showing black edges when changing settings.
- Fixed incorrect colors on translucent edges and shadows in PRISM Stickers.
- Fixed Save to My Effects remaining unavailable after Lens reconnected, along with display issues affecting some effects and controls.
- Fixed Lens effects not being applied correctly under certain multi-GPU configurations.
- Fixed Clock Widget hotkeys potentially causing brief video stalls.
- Fixed Spout2 capture failing to recover from certain initialization errors.

#### Installation and Stability

- Fixed saving or closing source properties dialogs potentially overwriting newer scene collection settings.
- Fixed issues affecting Mobile Connect during connection errors and application shutdown, and corrected installer firewall rules.
- Fixed excessive waiting in some installation, update, and shutdown scenarios, and added a notification when virtual camera setup fails.
- Fixed crashes in some application startup and shutdown scenarios.
- Strengthened protection of stream recovery data and resource access, and reduced sensitive information in diagnostic logs.


<a id="version-602"></a>
## 6.0.2 Beta

### Downloads

- [PRISM Live Studio 6.0.2 Beta for Windows](https://resource-prismlive.pstatic.net/202611021112/PrismLiveStudio_Setup_x64_6.0.2.117.exe)

### Changes

- Updated the OBS base version to 33.0.0 and adapted PRISM features accordingly.
- Expanded Lens support to more source types that support effect filters, with updated source selection and guidance in the Sidebar.
- Added support for applying multiple Lens filters to a single source.
- Updated Facebook login and channel connection authorization to request the permissions required for publishing videos.
- Added an 8000 kbps maximum video bitrate check for SOOP. Users are prompted to adjust their settings if the limit is exceeded.
- Improved application startup by deferring initialization of hidden platform chat Docks, reducing startup delays and interface stalls.
- Improved multistreaming preparation to reduce waiting time caused by preparing channels sequentially.
- Reduced memory and rendering overhead for GIF thumbnails, hidden animated backgrounds, Stickers, and Blur Filter.
- Streaming and Platforms
  - Fixed SRT / RIST streaming not using the selected audio track.
  - Fixed authentication errors during Custom RTMP streaming potentially triggering account logout and an application restart.
  - Fixed delayed responses causing incorrect streaming states after multistreaming preparation timed out.
  - Fixed conflicts between restoring YouTube rehearsal settings and starting another broadcast.
  - Fixed YouTube automatic start guidance appearing when its output had failed to start.
  - Fixed CHZZK and YouTube Docks not loading correctly when browser initialization was slow.
  - Fixed missing feedback when some platform refresh requests failed.
  - Fixed NAVER Shopping LIVE broadcasts potentially remaining active on the server after a stop request failed.
  - Fixed migrated or automatically linked channels being unexpectedly enabled by default.
- Dual Output
  - Fixed vertical source positions being reset when duplicating a scene.
  - Fixed scene collection operations not being properly restricted while Replay Buffer or other protected output states were active.
  - Fixed some Dual Output state updates being missed, causing the interface to become inconsistent with the actual output state.
  - Fixed some previously ended channels being restarted during crash recovery.
- Chat, Sources, and Lens
  - Fixed saved Chat Overlay My Themes potentially being overwritten after a theme data read failure.
  - Fixed existing chat sources displaying the wrong theme when a deleted My Theme ID was reused.
  - Fixed some chat My Themes and Sticker Recent items not being preserved when migrating scene collections from older versions.
  - Fixed Text Template potentially appearing blank after an interrupted configuration write.
  - Fixed repeated repainting of Text Template alignment and text case controls.
  - Fixed Scene Template thumbnails briefly disappearing during resource scans and image loading causing interface stalls.
  - Fixed Scene Template installation remaining available while Replay Buffer was active.
  - Fixed potential application termination during GIPHY playback under certain error conditions.
  - Fixed Sticker edits potentially being applied to the wrong source and incorrect loading states after selecting multiple Stickers.
  - Fixed inefficient Viewer Count font list loading and handling of invalid display dimensions.
  - Fixed YouTube Chat placeholder text being clipped in narrow Docks.
  - Fixed display issues with selected chat template borders, Background Template loading indicators, and Viewer Count notifications.
  - Fixed source properties dialog positioning, title bar visibility, and Background Template control layout at high DPI settings.
  - Fixed some source property sliders not updating correctly when clicking their tracks.
  - Fixed Lens slider rendering and disabled-state behavior.
  - Fixed Mobile Connect failing to establish a connection in some timeout scenarios and improved state reset after a phone disconnected.
- Installation, Stability, and Security
  - Fixed the installer potentially waiting too long during exit.
  - Fixed potential crashes involving windows and callbacks during application shutdown.
  - Fixed error handling when login animation loading failed or the application closed.
  - Strengthened update package authenticity verification, with clearer failure messages and cleanup of invalid cached packages.
  - Strengthened security handling for resource cache paths, malformed Mobile Connect data, and sensitive information in diagnostic logs.

<a id="version-601"></a>
## 6.0.1 Beta

### Downloads

- [PRISM Live Studio 6.0.1 Beta for Windows](https://resource-prismlive.pstatic.net/202610201049/PrismLiveStudio_Setup_x64_6.0.1.107.exe)

### Changes
- Dual Output
  - Fixed vertical output becoming black or losing its layout when scene collections were renamed, duplicated, or switched.
  - Fixed copied scene collections and nested scenes using incorrect layouts on the vertical canvas.
  - Fixed scene source selection becoming incorrect after repeated copy and paste operations, and referenced sources disappearing after PRISM restarted.
  - Fixed the vertical preview and projector remaining blank after video settings were reapplied without changing the video format.
  - Fixed platform-recommended settings overwriting encoding settings between horizontal and vertical outputs.
  - Fixed channels remaining in a preparing or loading state when a platform was still loading or one output failed to start.
  - Fixed a delayed output starting after Dual Output had already been stopped.
  - Fixed the resolution guide incorrectly reversing both Dual Output canvas orientations.
- Streaming and Platforms
  - Fixed streaming attempts failing without clear feedback when preparation took longer than expected.
  - Fixed closing PRISM or changing settings during Go Live preparation causing incorrect streaming state handling.
  - Fixed connected channel data potentially being lost when a save was interrupted or the disk was full.
  - Fixed repeated prompts after switching a YouTube rehearsal to a live broadcast.
  - Fixed YouTube scheduled broadcasts not restoring automatic start and end settings after a rehearsal failed to start.
  - Fixed YouTube Live Info remaining in a saving state or crashing when the window was closed while a save retry was still in progress.
  - Fixed YouTube tags and default language being cleared after refreshing and saving the scheduled broadcast list.
  - Fixed failed YouTube recovery deleting an existing broadcast, auto-start prompts blocking later Go Live processing, and failed preparation leaving a newly created broadcast behind.
  - Fixed Twitch channels being removed after a temporary authorization refresh failure during streaming.
  - Fixed Facebook reauthorization being considered unsuccessful too early, preventing a broadcast from starting.
  - Fixed NAVER Shopping LIVE cover images becoming uneditable after a failed live or rehearsal attempt.
  - Fixed NAVER Shopping LIVE creating or leaving a broadcast active after PRISM failed to start streaming.
  - Fixed NAVER Shopping LIVE remaining in a loading state after request errors and displaying duplicate or incorrect error prompts.
  - Fixed NAVER Shopping LIVE retaining the previous login session after a channel was disconnected.
  - Fixed high memory usage and reduced responsiveness when browsing many NAVER Shopping LIVE product images.
  - Fixed Stream settings remaining editable during some OBS-native streaming sessions.
- Chat, Sources, and Lens
  - Fixed All Chat losing messages received before the page finished loading and the CHZZK Chat Dock remaining logged out when browser initialization was slow.
  - Fixed some My Chat themes from older scene collection backups not being recovered during upgrade.
  - Fixed incorrect Chat source state restoration, empty platform lists, and potential crashes when deleting Chat sources.
  - Fixed potential crashes involving CHZZK Sponsorship sources during login, refresh, or deletion.
  - Fixed Text Template user content being reset when unavailable resources caused a fallback to the default template.
  - Fixed PRISM becoming unresponsive when a Sticker was applied after changing canvas orientation.
  - Fixed interface or video stalls while loading scene collections containing Stickers.
  - Fixed “+” characters in Clock Widget text becoming spaces after reload and placeholders appearing in the Spanish timer interface.
  - Fixed Scene Template resolution changes rounding 29.97 or 59.94 FPS to integer frame rates.
  - Fixed Mobile Connect showing an outdated QR code after disconnection or failing to reconnect after a temporary connection failure.
  - Fixed PRISM potentially crashing when malformed Mobile Connect streaming data was received from the local network.
  - Fixed Lens effect selections being lost or applied to another camera when multiple Lens views were open.
  - Fixed PRISM crashing when an undo or removal followed copying a Lens filter between cameras.
  - Fixed hidden Text Template, Viewer Count, and Audio Visualizer sources continuing to render.
  - Fixed cached resource processing causing interface stalls or repeated resource requests becoming stuck.
  - Fixed potential Browser Source crashes during scene changes or application exit.
  - Fixed source property previews accessing a source that was being deleted or replaced.
- Accounts, Interface, Installation, and Stability
  - Fixed failed account deletion requests being shown as successful.
  - Fixed notices and onboarding layers obscuring each other and reduced interface stalls caused by login and update animations.
  - Fixed incomplete Japanese agreement text and incorrect interface states in PRISM Plus and account controls.
  - Fixed repeated Contact Us submissions while offline, incorrect attachment layout for long filenames, and premature upload timeouts on slow connections.
  - Fixed PRISM crashing when exiting from the system tray while a dialog was still open.
  - Fixed PRISM taking too long to exit on poor networks, while NAVER Shopping LIVE images were loading, or immediately after opening an external link.
  - Fixed PRISM unexpectedly restarting after being force-closed in Windows.
  - Fixed user settings such as automatic recording being reset when upgrading from an older version.
  - Fixed scene collection names and custom Browser Dock settings being corrupted during upgrade.
  - Fixed update downloads stalling, invalid cached installation packages being reused, and installation cancellation becoming unresponsive.


<a id="version-600-beta"></a>
## 6.0.0 Beta

### Downloads

- [PRISM Live Studio 6.0.0 Beta for Windows](https://resource-prismlive.pstatic.net/202610121421/PrismLiveStudio_Setup_x64_6.0.0.99.exe)

### What’s New in 6.0.0 Beta

PRISM Live Studio 6.0.0 Beta is based on OBS Studio 32.2.2 and is currently available for Windows only.

### Changes

- Changed the OBS base version to 32.2.2 and adapted PRISM features accordingly.
- Changed the application interface and settings to follow OBS, with support for installing and using OBS plugins.
- Changed account and subscription management to use Account Hub, bringing existing account information, subscription details, and account actions into one place.
- Added Onboarding Guide to introduce the PRISM Sidebar, PRISM Streaming, and key feature entry points.
- Changed the Help menu structure to provide separate OBS Help and PRISM Help & Support menus.
- Changed the PRISM Sidebar layout and feature entry points to integrate with the OBS main window.
- Changed how PRISM Sources are added and edited: sources are added through Add Source, managed in the OBS Sources list, and edited through the PRISM Sidebar or property windows. Adapted sources include Text Template, Chat Overlay, Clock Widget, Audio Visualizer, Background Template, Viewer Count Widget, Mobile Connect, and Sticker.
- Changed the Source Toolbar to retain only the common Properties and Filters actions, removing PRISM-specific action buttons.
- Changed the Scene Template workflow to import templates into the current Scene Collection and apply them immediately.
- Changed the multistreaming and channel management workflow to integrate existing platform features into the PRISM Streaming Dock. Platform Live Info is managed through channel tabs in Manage Broadcast.
- Changed the Resolution Guide entry point from the Sidebar to Manage Broadcast.
- Changed Output Setting Guide to display recommended resolutions, FPS, and official platform guides together, with an option to apply the recommended settings.
- Changed the Lens workflow to provide face retouch, background removal, Color Filter, Funny Face, Advanced Camera Settings, and other effects through Lens Filter on camera sources within the application. The existing PRISM Lens application is retained, but its previous capture-device workflow is no longer supported in the Beta. Lens effects must be configured again within the Beta.
- Changed Blur integration to use the OBS Filter system.
- Changed Spout2 integration to use the OBS plugin system, providing Source, Output, Filter, and Tools menu entries.
- Added more theme options by incorporating OBS Themes alongside the existing PRISM theme.
- Changed the Contact Us form and feedback flow, adding separate routing for PRISM and OBS issues.
- Updated the Terms of Use and Privacy Policy links used during registration.
- The installer continues to support PRISM's seven languages, using the system language by default and English when the system language is unsupported.
- Added migration support for existing scenes, settings, channel lists, and some sources' My / Recent data. Unsupported sources cannot be migrated and are removed. Some Docks, including Chat and Control Panel, may require users to log in again.
- Removed system file association for `.psc` files. Double-clicking these files no longer opens PRISM Live Studio.


## Community

- [Github issues](https://github.com/naver/prismlivestudio/issues)  
- [GitBook](https://guide.prismlive.com/)  
- If you have any question, please contact us by [mail:prismlive@navercorp.com](mailto://prismlive@navercorp.com)
