# Control socket

The client can be driven from a shell: logging in, selecting a character,
walking, fighting, chatting, reporting what it sees and taking screenshots,
without anyone at the keyboard. It is developer tooling for scripted tests and
demos, off unless a launcher asks for it, and it works the same on Windows and
on Linux.

## Turning it on

The socket is a build-time feature. It exists only in a client configured
with `ENABLE_CONTROL_SOCKET=ON` — the `-mueditor` presets and hand-configured
developer builds; the plain presets (`linux-x64`, `windows-x64`, …) and any
build that does not opt in compile none of it, and the `control_socket_leak`
test in their test suite proves it. A player build ignores the variable
below entirely: no socket, no log line, no change of behaviour.

In a build that has it, the client opens the socket only when
`MU_CONTROL_SOCKET` names a path:

```sh
MU_CONTROL_SOCKET=/run/user/1000/clients/default.sock ./Main /u127.0.0.1 /p44405
```

Without the variable nothing is created, nothing is logged and the client
behaves exactly as before. With it, the client logs one line
(`control socket listening on <path>`), creates the socket with owner-only
permissions (`0600`), replaces a stale file left behind by a crashed client,
and removes the file when it exits normally.

An option would not do: the client's command line is split on spaces, so a
path containing one could not be passed.

## Talking to it

Newline-delimited JSON, one request object per line, exactly one response per
line:

```sh
printf '{"cmd":"ping"}\n' | socat - UNIX-CONNECT:/run/user/1000/clients/default.sock
{"ok":true,"result":{"build":"Sep 16 2026 16:22:46","scene":"world"}}
```

- A request carries `cmd` and the command's own fields, plus an optional `id`
  that is echoed back so a caller can match answers to requests. `id` belongs
  to the framing: a drop is named by `item`, not by `id`.
- A response is `{"ok":true,"result":{…}}` or
  `{"ok":false,"error":"<code>","message":"…"}` — with a `result` holding the
  progress made when a long-running command was interrupted or timed out.
- Requests are served on the main thread, once per frame, after the packets of
  that frame have been processed. Nothing runs concurrently with game logic.
- Several connections may be open at once. Commands that drive the character
  (`login`, `move`, `attack`, …) run one at a time: a second one interrupts the
  first, whose caller is told how far it got. Reading commands (`state`,
  `events`, `wait-for`, `screenshot`) run alongside.

Error codes: `bad_request`, `unknown_command`, `wrong_scene`, `busy`,
`interrupted`, `timeout`, `not_connected`, `login_failed`, `no_such_character`,
`no_such_skill`, `not_in_view`, `not_attackable`, `no_path`, `not_allowed`,
`warp_refused`, `skill_refused`, `insufficient_mana`, `not_pickable`,
`empty_slot`, `move_refused`, `failed`.

## Commands

| Command | What it does |
|---|---|
| `ping` | build identifier and current scene |
| `scene` | which screen the client is on: `login`, `character_list`, `world`, … |
| `state` | the character and everything around it (see below) |
| `nearby` | the objects the client can see |
| `events` (`since`, `follow`) | recorded events, or a live stream of them |
| `wait-for` (`event`, `match`, `timeout`) | block until a matching event arrives |
| `screenshot` (`out`) | capture the next frame to a path; without `out` it names itself, uniquely per capture |
| `hotkey` (`key`) | press one game key for a frame: `esc`, `i`, `home`, `f1`, … |
| `click-ui` (`x`, `y`, `button`) | click a window pixel (`left` by default) |
| `type` (`text`, `enter`) | type text into the focused field, optionally submitting it |
| `login` (`account`, `password`, `server`) | server selection, credentials, character list |
| `select-char` (`name` or `slot`) | enter the world with that character |
| `logout`, `quit` | back to the character list; close the client |
| `move` (`x`, `y`) | walk there with the client's own path finder |
| `warp` (`gate`) | use a warp-list entry by name |
| `teleport` (`x`, `y`, `map`) | the game master's own move command; `map` is an index, the current map when omitted |
| `attack` (`target`, `times`, `interval`) | plain attacks on an id or character name |
| `skill` (`skill`, `target`) | cast a skill the character owns |
| `pickup` (`item`) | walk to a drop and take it, by the id `nearby` reports for it |
| `use` (`slot`), `equip` (`slot`, `target_slot`) | inventory actions |
| `say` (`text`), `whisper` (`name`, `text`) | chat, including `/` commands |
| `party` (`action`, `target`) | `invite`, `accept`, `decline`, `leave` |
| `halt` | stop the walk or repeated attack in progress |
| `inject` (`hex`) | run one decrypted server→client packet through the receive path (debug-ui) |
| `net` (`action`) | `mute`/`unmute`/`status`/`reset`: drop every real incoming packet while muted (debug-ui) |
| `ui` (`action`, `window`, `raw`, `name`, `percent`, `guard`) | `list`, `show`/`hide`/`toggle`, opt-in `guarded_show`/`guarded_hide`, `theme`, `scale` (debug-ui) |
| `window` (`action`, `width`, `height`) | `status`, `resize` the game window (debug-ui) |
| `hover` (`x`, `y`) | move the pointer to a window pixel and leave it there (debug-ui) |
| `render` (`world`) | `on`/`off`/`status`: skip the 3D world so only the UI is drawn (debug-ui) |

`state` reports the scene and account on every screen, and in the world adds:
character name, class, level, experience, zen, HP/mana/SD/AG with their
maxima, map number and name, position, alive flag, safe-zone flag, current
target, the skills the character owns, equipment, inventory, buffs, party and
`nearby`.

### Synthetic input

`hotkey` and `click-ui` inject a key or a click onto the client's own SDL
event queue, so it takes the same path a physical key or click takes: the
event loop offers it to the UI first and to the legacy readers when the UI
leaves it unclaimed. That is what makes both work regardless of which UI a
window is built in — an RmlUi document is driven by SDL events alone, and
never sees anything written straight into the legacy input globals, so a
scripted session can press an RmlUi window's tabs, dropdowns and buttons as
well as the original HUD's. Nothing else about the window system is touched:
no focus change, no OS-level input, so the human can keep working elsewhere
while a scripted client plays. A click is preceded by a pointer move to its
own position, because a press is dispatched to whatever the pointer last
moved over. Coordinates for `click-ui` are the pixels of the client area, the
same space a `screenshot` image is in, so a script can capture, locate and
click. Key names are case-insensitive: the letters, the digits, `esc`,
`enter`, `tab`, `space`, `backspace`, `home`, `end`, `insert`, `delete`,
`pageup`, `pagedown`, `up`, `down`, `left`, `right`, `printscreen` and
`f1`–`f12`; anything else answers `bad_request`. A press and its release are
always separated by a rendered frame, since the legacy readers consume
press/release edges per frame.

A key press carries no character — in this client characters arrive as their
own event — so `type` is what fills a text field: the account and password
boxes, a character or guild name, a chat line. It sends the text to whatever
field has keyboard focus (click it first), and with `enter` true follows it
with a Return press one frame later, for fields that submit on it. Printable
text only, at most 256 bytes; a newline or a tab is a `hotkey`, and UTF-8 goes
through unchanged.

All three answer once the release frame has run; a second injection while one
is in flight answers `busy`, while a walk or an attack in flight is left alone
— injecting a key is an observation of the act slot, not a claim on it. The
sequence follows *rendered* frames, so an injection sent to a client that is
not rendering (the occluded-window case below) answers `timeout` and is
dropped rather than delivered late. Not covered: key chords, drags.

### Debug-UI: reproducible UI states without a server

The six commands marked *debug-ui* (`App/Control/ControlCommandsDebugUi.cpp`)
exist so that a UI can be photographed in a known state, the same state
every run, on any build that carries them:

- `inject` takes a complete, decrypted `C1`/`C2` packet as hex
  (`"C1 0A F8 ..."`, whitespace optional) and processes it on the main thread
  exactly as a received packet — the length byte(s) must match, `C3`/`C4`
  are refused, and so is a client without a game-server connection
  (`not_connected`). Nearly every window state the client can show is a
  stored packet (gens membership, party, guild, duel request, quests, buffs),
  so this is how a scenario produces one deterministically.
- `net mute` drops every real incoming packet on the network thread from
  then on (`status` reports how many, by head code, `reset` zeroes the
  counts; `unmute` resumes). The session stays open and the client still
  sends, so the server keeps its view — the world on screen simply stops
  changing.
- `ui list` names every main-scene window (`INTERFACE_*` without the prefix,
  lowercased: `inventory`, `character`, `gensranking`, …) with its
  registered/visible flags and, where the build has RmlUi, the active theme.
  Instrumented builds additionally return `observability.version: 2`, with
  `messagebox_active` (native dialog stack, independent of manager visibility),
  `legacy_popup_active`, `generic_confirm_active`/`generic_menu_active`,
  `helper_active`, `input_focused` and `friend_open_allowed`.
  `input_focus_owner` is `"chatinputbox"` only for focus held by that exact
  registered panel's chat/whisper field; otherwise it is null (unidentified).
  Activity is boolean when observable and `null` when unavailable; never treat
  missing/null as inactive. `generic_dialogs_supported: false` explicitly marks
  a build without those RmlUi mechanisms; their activity fields stay null.
  These read-only observations do not certify that every modal/input mechanism
  is covered. Consumers must also check scene, registry/transaction state and
  any other prerequisites of their operation. Repeating `ui list` does not
  dismiss a dialog or change focus.
  `ui show|hide|toggle <window>` goes through the window system's own
  `Show`/`Hide` (dock-neighbour placement, group hiding, refusals included);
  `raw: true` flips only the manager flag. `ui theme <name>` performs the
  `$theme` console command's sweep; a build without RmlUi answers `failed`.
  `ui scale <percent>` sets `UIScalePercent` for the session and re-applies
  the current window size. `ui` needs the world for everything but
  `theme`/`scale`.
- `window resize <w> <h>` resizes the (windowed) game window through the
  same path as the options dialog and answers with the size the window
  really got — a tiling compositor may decide otherwise. `window status`
  reports size and scale.
- `hover x y` pushes one mouse-motion event through SDL's own queue, so the
  legacy pointer variables *and* (where present) the RmlUi context see it;
  the pointer stays there until the next motion, which is what a tooltip
  needs.

- `render world off` skips the 3D world (terrain, objects, characters,
  effects, items) in the main scene; the UI renders over the clear colour,
  so a screenshot of two builds differs only where the UI does. `on`
  restores it, `status` reports.

`ui`, `window`, `hover` and `render` answer after two rendered frames, so
the next command — usually `screenshot` — sees the result.

## Events

The packet handlers record what a scenario asserts on. Each event carries a
strictly increasing `seq`, a UTC `time` and its own fields:

| Event | Fields |
|---|---|
| `hit` | `direction` (`dealt`/`received`), `attacker`/`target` `{id,name,kind}`, `damage`, `shield_damage`, `critical`, `missed` |
| `killed` | `victim`, `killer` |
| `stat` | `stat` (`life`, `mana`, `sd`, `ag`, `experience`, `level`), `value`, `max` |
| `chat` | `sender`, `text`, `kind` |
| `drop` / `drop_gone` | `id` (the id `pickup` takes), `item`, `position` / `reason` |
| `map` | `map`, `map_name`, `position` |
| `scene` | `scene` |
| `view_enter` / `view_leave` | `object` |
| `party` | `change`, `name` |
| `disconnect` | `reason` |
| `error` | `command`, `error`, `message` |

The last 2,048 events are kept. Names and fields are plain lower snake case,
so a test script can parse them next to a server-side event stream.

## Security posture

**Build-time gate first.** A player build contains no control code at all:
`ENABLE_CONTROL_SOCKET` is off by default, the plain presets and CI keep it
off, none of `App/Control/` or the local-socket transport is compiled, and
the activation variable's name does not appear in the executable. What a
player can reach by setting an environment variable is therefore nothing.
What stays in every build is the mouse-free automation the in-game helper
already had (`GameLogic/Automation`) and the extracted login/character entry
points — refactorings of existing code, not new surface. The gate removes the
ready-made API and the flip-a-switch path; it does not defend against a
patched or rebuilt binary.

Within a developer build the socket is unauthenticated: any process of the
same user can drive the client, like any other local developer endpoint. It is
a filesystem socket with mode `0600` in the user's runtime directory, never a
network address, never enabled from `config.ini`, and never on unless the
launcher sets the variable.

## Keeping the taps when `WSclient.cpp` changes

The event recorders are one-line calls named `App::Control::Events::Record…`,
sitting at the end of the packet receive functions in
`src/source/Network/Server/WSclient.cpp` (hits, deaths, experience, stats,
chat, whisper, drops appearing and vanishing, view enter/leave, party changes,
logout) plus the scene and map watcher in `App/Control/ControlServer.cpp`.
When one of those functions is rewritten:

1. `rg -c 'App::Control::Events::' src/source/Network/Server/WSclient.cpp` —
   the count is 19; a lower one means a tap was dropped. The debug-ui mute
   is one more call, `App::Control::Net::DropIncoming`, at the top of
   `HandleIncomingPacket`.
2. Re-run the live checks that cover the dropped tap (a fight records `hit`,
   `killed` and `stat`; a pickup records `drop` and `drop_gone`).

## Notes from the field

- The client's main loop stops running while its window is fully occluded (the
  compositor stops sending frame callbacks), and with it the socket stops
  answering. Two clients side by side must both stay visible; tiled windows
  answer `ping` in about 4 ms.
- The server's speed check bans an account after four warnings in an hour, so
  every command that walks paces its steps (300 ms between walk packets,
  250 ms between automation steps). Do not remove that pacing.
- Attacks are refused inside a safe zone; `state`'s `safe_zone` flag says when
  the character is in one, and `attack` answers `not_allowed` there.

### Guarded comparison operations

Instrumented comparison builds report pending inventory/character opening quest
actions, carried items, native pending events, legacy message windows, friend
children and identified system-menu state. `ui list` includes an opaque `guard`
string describing the observed UI snapshot. Do not construct it yourself.

`ui` actions `guarded_show` and `guarded_hide` accept `window` and that `guard`.
They refuse changed/unknown state, unsupported panels and transaction/modal state
before invoking the ordinary synchronous Show/Hide path. `system_menu` is a special
name for opening the normal system menu or cancelling that identified menu only.
These operations never execute menu choices. Existing unguarded commands retain
their original behavior. A successful request still needs visibility and full-frame
verification: an unavailable natural route is not automatically an opening pass.
Only closing the registered, visible chat-input panel may retain its own identified
native input focus; other focused or unidentified inputs still refuse. The old-side
adapter does not yet identify chat focus and therefore refuses that focused close.

The guard covers all reported window visibility, including autonomous HUD flags.
A stale refusal requires a fresh observation and full prerequisite revalidation,
not a blind retry. The settle reply reports the requested action, not its outcome:
query `ui list` again after settling and verify the expected visibility/modal state.
Native pending events may remain reported after a dialog is popped; never clear or
ignore them to force a guarded operation. RmlUi focus is covered by the observed
generic dialogs and refused option window, not a general DOM-focus observation.

Ordinary callbacks are preserved, so these operations are not packet-free. Opening
party requests the party list; closing the quest journal (including opening inventory
while the journal is open) sends a close-NPC request. Opening friends can request the
friends list and clears the mail alert. Character/inventory opening is refused when
its observed tutorial-quest callback would change progression. No callback is
suppressed to obtain a screenshot.
