# MANARA theme

* `src/resources/themes/Manara.json` — chat bubble theme (orange/blue from the
  logo), selectable in Settings → Theme. Built from ClassicSlate with the MANARA
  palette; follows the same schema (themes/SCHEMA.md). It is the default
  (`IDS_THEME_VAL`). `ClassicSlate` remains the compiled-in fallback: if a theme
  fails to load, `core/theme.cpp` falls back to it, so a mistake in Manara.json
  cannot leave the chat without a theme. Tests name ClassicSlate explicitly and
  do not depend on the default.
* `src/resources/qss/manara-light.qss` / `manara-dark.qss` — application style
  sheets using the real object names (MainWindow, frame, lblUserName, lblStatus,
  txtNote, wgtToolBar, tvUserList, ChatWindow, txtMessage, toolBar, splitter,
  TransferWindow, lvTransferList, btnClear, btnClose, SettingsDialog,
  lvCategories). To try one, copy it to the user theme folder as `custom.qss`
  (read at startup by `applyCustomQss`).
* Tokens: `docs/design/tokens.json`. Icons: `docs/design/icons/`
  (the ICO also replaces `src/app/lmc.ico` and `src/resources/icons/lmc.ico`).

Status: JSON is valid and keeps the schema's shapes; QSS syntax and the look
have **not** been checked in a running Qt GUI (this build had no QtGui).
