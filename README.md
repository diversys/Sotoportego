# Sotoportego

A native VPN client for [Haiku OS](https://www.haiku-os.org/), written in C++
against the Haiku/BeAPI.

> **Milestone 1 — compiling skeleton.** This is the architectural skeleton: it
> exercises the IPC and backend seams end to end with a *stub* backend. There
> is **no real VPN logic yet** (no OpenVPN, no routing, no DNS). See
> [What's stubbed](#whats-stubbed) and [Milestone 2](#milestone-2-plan).

A *sotoportego* is a Venetian covered passage that connects two streets by
tunneling under a building — a fitting name for a tunnel you walk through.

## Architecture

Sotoportego is **privilege-separated** and **backend-pluggable** from day one,
even though most of the machinery is still stubbed.

```
┌──────────────┐   BMessage IPC   ┌──────────────────────────────┐
│ sotoportego_ │ ───────────────► │ sotoportego_server (daemon)  │
│ cli (client) │ ◄─────────────── │  - owns VPN lifecycle        │
└──────────────┘  status updates  │  - holds one VPNBackend      │
   (later: GUI,                   │  - (later) routing + DNS     │
    Deskbar replicant)            │      as a shared service     │
                                  │                              │
                                  │   ┌────────────────────────┐ │
                                  │   │ VPNBackend (interface) │ │
                                  │   │   └ OpenVPNBackend     │ │
                                  │   │     (stub this MS)     │ │
                                  │   │   (later: WireGuard,   │ │
                                  │   │    IPSec)              │ │
                                  │   └────────────────────────┘ │
                                  └──────────────────────────────┘
```

- **The daemon owns everything.** A background server process owns the VPN
  lifecycle, and will own routing, DNS, and credentials. It is the single
  source of truth.
- **Clients are front-ends.** The CLI now, and a GUI + Deskbar replicant later,
  are separate processes that drive the daemon over `BMessage`. The daemon runs
  as the normal user for now, but the **process boundary is real** so privilege
  escalation can be added later without reshaping anything.
- **Backends are pluggable** behind the abstract `VPNBackend` interface.
  OpenVPN is first; WireGuard and IPSec come later. Route/DNS handling will be a
  **shared service of the daemon**, not duplicated per backend.

## Layout

```
src/
  common/     Shared types & wire protocol (linked into every binary)
    VPNState.{h,cpp}     Connection state enum + name helper
    VPNStats.{h,cpp}     Throughput counters (archivable to BMessage)
    VPNProfile.{h,cpp}   A connection profile (archivable to BMessage)
    VPNProtocol.h        Message 'what' codes, field names, app signatures
  backend/    The pluggable backend seam
    VPNBackend.{h,cpp}   Abstract interface (a BHandler) + notify helpers
    OpenVPNBackend.{h,cpp}  Stub backend: scripted state machine, no real VPN
  server/     The daemon (a BApplication / BLooper)
    SotoportegoServer.{h,cpp}
    main.cpp
    Sotoportego_server.rdef
    Makefile
  cli/        sotoportego_cli — test client that proves the IPC round-trip
    main.cpp
    Sotoportego_cli.rdef
    Makefile
Makefile      Top-level: recurses into src/server and src/cli
```

### App signatures

| Binary               | Signature                                      |
| -------------------- | ---------------------------------------------- |
| `sotoportego_server` | `application/x-vnd.VePro-SotoportegoServer`    |
| `sotoportego_cli`    | `application/x-vnd.VePro-SotoportegoCLI`        |
| (future GUI)         | `application/x-vnd.VePro-Sotoportego`          |

## Building

Requires **Haiku** with the development tools (the `makefile-engine` lives at
`/system/develop/etc/makefile-engine`). The Makefiles are standard Haiku
Generic Makefiles.

```sh
make            # builds both binaries
make clean      # cleans both

# or build a single component:
make -C src/server
make -C src/cli
```

> **Note:** the project only builds and runs on Haiku — it links `libbe.so` and
> uses the BeAPI. It will not build on Linux/macOS. (During development on a
> non-Haiku host the sources were syntax-checked against mock BeAPI headers,
> but the authoritative `make` and run must happen on Haiku.)

The binaries are produced in their respective `src/*` directories.

## Running

```sh
# From a terminal on Haiku:
./src/cli/sotoportego_cli
```

`sotoportego_cli` will:

1. Launch the daemon (`sotoportego_server`) if it is not already running, via
   `be_roster`.
2. Subscribe for status updates and ask the daemon to connect using a demo
   profile.
3. Print every state and stats update it receives.
4. After reaching **Connected**, linger briefly (to observe a throughput tick),
   then ask the daemon to disconnect and exit once **Disconnected**.

Expected output (the stub walks the state machine on timers):

```
[cli] connecting to 'Demo Profile' via daemon...
[cli] state: Connecting
[cli] state: Authenticating
[cli] state: Connected
[cli] stats: in=12288 bytes  out=3072 bytes
[cli] requesting disconnect
[cli] state: Disconnected
[cli] round-trip complete; exiting
```

The daemon logs its side to its own stdout (visible if you start it manually
first). This round-trip is the proof that IPC + the backend seam work end to
end.

## What's stubbed

Milestone 1 deliberately implements **plumbing only**. The following are stubbed
and marked with `TODO(milestone-2)` (or `TODO`) in the source:

- **OpenVPNBackend** does *not* talk to OpenVPN. It walks
  `Connecting → Authenticating → Connected → …` on a `BMessageRunner` timer and
  emits fake throughput, so the IPC is fully testable.
- **Routing** — none. Will be a shared daemon service.
- **DNS** — none. Will be a shared daemon service.
- **OpenVPN management interface** — not implemented; this is the core of
  Milestone 2.
- **Privilege escalation** — none. The daemon runs as the normal user; only the
  *process boundary* exists today.
- **`.ovpn` parsing** — none. `VPNProfile` carries a config path but nothing
  reads it yet.
- **Credential storage** — none.

## Milestone 2 plan

**Goal: a real OpenVPN connection driven through the management interface.**

1. **Process management.** Have `OpenVPNBackend` spawn the `openvpn` binary as a
   child process, pointed at a generated/loaded config, with
   `--management 127.0.0.1 <port>` (or a unix socket) and
   `--management-hold`. Supervise the child (detect exit, surface errors).
2. **Management interface client.** Implement a small client for OpenVPN's
   management protocol: connect to the socket, send `state on`,
   `bytecount <n>`, `hold release`, and parse the asynchronous `>STATE:`,
   `>BYTECOUNT:`, `>PASSWORD:`, and `>LOG:` notifications. Map `>STATE:` values
   (`CONNECTING`, `AUTH`, `GET_CONFIG`, `CONNECTED`, `RECONNECTING`, `EXITING`)
   onto `VPNState`, and `>BYTECOUNT:` onto `VPNStats`. Replace the scripted
   timer with this event source.
3. **`.ovpn` parsing.** Parse profiles into the fields `OpenVPNBackend` needs;
   keep `VPNProfile` as the in-memory representation.
4. **Credentials.** Answer `>PASSWORD:`/`>USERNAME:` prompts; store secrets via
   the Haiku keystore (`BKeyStore`) rather than in the profile.
5. **Routing + DNS as a shared daemon service.** Introduce a `RouteManager` and
   `DNSManager` owned by the daemon (not the backend): apply pushed routes and
   `dhcp-option DNS`, and restore them on disconnect. This is where privilege
   separation starts to matter — isolate the privileged operations behind a
   narrow interface so a future helper can perform them.
6. **Robustness.** Reconnect/backoff handling, clean teardown on daemon exit,
   and surfacing precise error detail strings to clients.

## License

MIT — see [LICENSE](LICENSE).
