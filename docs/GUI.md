# Sotoportego — GUI design direction

Target for **v1.0**: a professional, elegant, "corporate" interface that is
nevertheless **unmistakably Haiku** — built with the native Interface Kit and
Layout Kit, not a faux-web or cross-platform toolkit look. Clean, calm,
information-first; no gradients-for-their-own-sake, no clutter.

This document is the visual contract the GUI client is built against. The GUI
is **just another client of the daemon** (same BMessage protocol as the CLI),
so none of this couples to the VPN core.

## Principles

1. **Native first.** `BWindow` / `BView` / `BButton` / `BMenuBar`, laid out with
   the **Layout Kit** (`BLayoutBuilder`, `B_USE_DEFAULT_SPACING`,
   `B_USE_WINDOW_SPACING`). The window resizes and respects font size like any
   first-class Haiku app.
2. **System palette + one accent.** Colors come from `ui_color()`
   (`B_PANEL_BACKGROUND_COLOR`, `B_PANEL_TEXT_COLOR`, `B_CONTROL_*`) so the app
   honours the user's chosen color scheme and dark mode. We add exactly **one**
   semantic accent, used only for the connection-status indicator.
3. **Status-first hierarchy.** The single most important thing — *are you
   connected?* — is the largest, highest-contrast element. Everything else
   (server, throughput, profiles) is secondary.
4. **Generous, consistent spacing.** Default spacing everywhere; group related
   controls in `BBox` sections. No cramped toolbars.
5. **Calm motion.** State changes update text/indicator in place; no spinners
   competing for attention. A subtle pulse on "Connecting" at most.

## Semantic accent (the only custom colors)

The status indicator (a small filled circle drawn in a `BView`) and the status
label use one color per state. These are the *only* hand-picked colors; tuned
to read on both light and dark system backgrounds.

| State            | Meaning           | Color (approx)        |
| ---------------- | ----------------- | --------------------- |
| Disconnected     | idle              | neutral grey `#7F7F7F`|
| Connecting/Auth  | in progress       | amber `#E0A030`       |
| Connected        | success           | green `#3DA35D`       |
| Reconnecting     | recovering        | amber `#E0A030`       |
| Error            | failure           | red `#C8463C`         |

## Main window layout

```
┌─ Sotoportego ───────────────────────────────────────────────┐
│ App  Connection  Help                          (BMenuBar)    │
├───────────────┬─────────────────────────────────────────────┤
│ Profiles      │  ●  Connected                  ← status      │
│ ┌───────────┐ │     vpn.example.com:1194        (big label   │
│ │ Demo      │ │                                  + indicator)│
│ │ Work      │ │  ┌─ Session ─────────────────────────────┐  │
│ │ ...       │ │  │ Since      14:32:07                    │  │
│ │           │ │  │ Download   12.3 MB   ↓                 │  │
│ │(BListView)│ │  │ Upload      3.1 MB   ↑                 │  │
│ └───────────┘ │  └───────────────────────────────────────┘  │
│  [+]  [–]     │                                              │
│               │              [   Disconnect   ]  ← primary   │
└───────────────┴─────────────────────────────────────────────┘
```

- **Left column** — a `BBox` "Profiles" wrapping a `BListView` in a
  `BScrollView`, with small add/remove buttons beneath. (Profile editing arrives
  with the profile/.ovpn work; for now the list shows the demo profile.)
- **Right column** — top: the **status block** (an indicator `BView` + a large
  `BStringView` for the state, and a secondary line for the server). Middle: a
  `BBox` "Session" with connected-since and throughput. Bottom: the **primary
  action button**, which reads *Connect* / *Disconnect* and reflects progress
  (disabled + "Connecting…" while transitioning).

## Components beyond the main window (later in v1.0)

- **Deskbar replicant** — a small tray indicator showing the status accent
  color and a menu to connect/disconnect the last profile. Also a daemon
  client; shares the same protocol.
- **Profile editor** — a modal/window to create and edit `VPNProfile`s
  (name, server, port, backend, credentials), landing with the profile work.
- **Preferences** — start-on-login, default profile, notifications.
- **Notifications** — use the Haiku notification system (`BNotification`) for
  connect/disconnect/auth-failure when the window is in the background.

## Why this reads as "corporate" on Haiku

Restraint. A single accent, system typography and colors, strong alignment, and
a clear primary action. It looks deliberate and trustworthy without fighting the
platform — the same reason native Haiku apps like the ones in the Tracker/Deskbar
ecosystem feel coherent.
