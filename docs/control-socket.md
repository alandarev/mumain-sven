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
`empty_slot`, `move_refused`, `not_open`, `failed`.

## Commands

| Command | What it does |
|---|---|
| `ping` | build identifier, the git `commit` the client was built from (`commit_changed` when tracked files differed from it), and the current scene |
| `scene` | which screen the client is on: `login`, `character_list`, `world`, … |
| `state` | the character and everything around it (see below) |
| `nearby` | the objects the client can see |
| `events` (`since`, `follow`) | recorded events, or a live stream of them |
| `wait-for` (`event`, `match`, `timeout`) | block until a matching event arrives |
| `screenshot` (`out`, `quality`) | capture the next frame as a JPEG to a path; without `out` it names itself, uniquely per capture; `quality` 1 to 100, default 100 |
| `hotkey` (`key`) | press one game key for a frame: `esc`, `i`, `home`, `f1`, … |
| `click-ui` (`x`, `y`, `button`) | click a window pixel (`left` by default) |
| `hover-ui` (`x`, `y`) | move the pointer to a window pixel with no button, as physical motion: RmlUi hover and the game's own pointer (native hover, tooltips) both follow it |
| `drag-ui` (`x`, `y`, `to_x`, `to_y`, `button`) | press at one window pixel, move to the other over eight frames with the button held, release there |
| `type` (`text`, `enter`) | deliver committed UTF-8 to the focused field; optional boolean `enter` submits on a later frame |
| `ui` | the open windows by name — `inventory`, `inventory_extension`, `character`, `trade`, `storage`, `storage_extension`, `mix`, `npc_shop`, `lucky_item`, `chat_input`, `party`, `command`, `my_shop`, `purchase_shop`, `npc_quest` (the quest dialog of Sebina, Marlon and Devin), and `message_box` while a dialog waits for Enter or Esc — and the window pixels of the named elements that are shown: `trade.confirm`, `trade.zen`, `inventory.repair`, `inventory.my_shop`, `npc_shop.repair`, `npc_shop.repair_all`, `my_shop.title`, `my_shop.open`, `my_shop.close`, `command.trade`, `command.purchase`, `command.party`, `npc_quest.answer.0` and on (the rows of the quest dialog's answers), `npc_quest.complete`, `npc_quest.close`, `character.stat.strength` (`agility`, `vitality`, `energy`, `command`: the character window's "+" buttons, while there are level-up points) |
| `slot-pixel` (`grid`, `slot`) | the window pixel of a slot's square: `inventory` and `equipment` (the slot numbers `state` reports), `trade`, `trade_partner`, `storage`, `mix`, `npc_shop`, and the personal shops `my_shop` and `purchase_shop` (slots 204 and up, as `state` and the server number them); `not_open` while that window is closed, `bad_request` for a slot the grid does not have. `equipment` reads the current visible prepared slot DOM: character/Armory for embedded-equipment themes, otherwise inventory; character-only Armory needs no open bag. Hidden, missing or unmeasured equipment slots answer `bad_request`, never cached coordinates; querying performs no layout or gameplay synchronization |
| `wheel` (`notches`, `x`, `y`) | scroll `notches` (-5..5, not 0; positive is away from the user) one per frame, optionally after moving the pointer to a window pixel |
| `drag` (`from`, `to`, `button`, `steps`) | press at `from` `[x, y]`, move to `to` over `steps` frames (1..30, default 8), release there (`left` by default) |
| `login` (`account`, `password`, `server`) | server selection, credentials, character list |
| `select-char` (`name` or `slot`) | enter the world with that character |
| `logout`, `quit` | back to the character list; close the client |
| `select-slot` (`slot`) | on the character list, select that character as a single click on it does (Connect/Delete enabled); sends nothing, never enters the world |
| `server-list` | on the character list, the system menu's Server Select (a logout back to server selection); answers once the server list is shown |
| `select-server` (`server`) | on the server list, the `login` server selection up to the login form, then stops: no credentials |
| `move` (`x`, `y`) | walk there with the client's own path finder |
| `warp` (`gate`) | use a warp-list entry by name |
| `teleport` (`x`, `y`, `map`) | the game master's own move command; `map` is an index, the current map when omitted |
| `attack` (`target`, `times`, `interval`) | plain attacks on an id or character name |
| `skill` (`skill`, `target`) | cast a skill the character owns — with `target` it is aimed at that object, without one it is cast where the character stands |
| `pickup` (`item`) | walk to a drop and take it, by the id `nearby` reports for it |
| `use` (`slot`), `equip` (`slot`, `target_slot`) | inventory actions |
| `say` (`text`), `whisper` (`name`, `text`) | chat, including `/` commands |
| `party` (`action`, `target`) | `invite`, `accept`, `decline`, `leave` |
| `trade` (`action`, `target`) | `request` a trade with another player at most one tile away (`not_allowed` otherwise), or `cancel` the open one; the partner accepts with the Enter key, and items go in with `click-ui` |
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
target, the skills the character owns, equipment, inventory, buffs, party,
the open trade (partner, both offers with their items and zen, both confirm buttons and `my_confirm_wait`, the frames until my button takes clicks again, or `null`) and
`nearby`. An item carries `slot`, `name`, `level`, `durability`,
`max_durability`, and its `width` and `height` in inventory squares; one that
covers several squares is listed once for each of them. While a click would
repair it (the inventory's repair mode, or an NPC that repairs), a worn item
also carries `repair_price`, and while an NPC shop is open an inventory item
carries `sell_price`: both as the item's tooltip shows them.

The shops: `npc_shop` is the open NPC shop (`repair_shop`, `tax_rate`,
`repair_all_price` at an NPC that repairs, and its goods with `price`, tax
included) or `null`; `my_shop` is the player's own personal shop (`open`, the
`title` in its title field, its goods with `price`), `purchase_shop` the
personal shop the player looks into (`seller`, `title`, goods with `price`) or
`null`. The goods of the personal shops whose price isn't in zen also carry the
`price_item_type` of their price, as the server encodes it: the item (group in
the highest 4 bits, number in the others) or the account coin (`0xFFF0` WCoin
(C), `0xFFF1` WCoin (P), `0xFFF2` Goblin Points); see
[personal-shop.md](personal-shop.md). `repair_mode` says whether a click
repairs instead of picking up.

For the quests and their rewards: `class_name` (e.g. `Blade Knight`; `class`
is the client's number, e.g. 1 Dark Knight, 8 Blade Knight, 12 Blade Master),
`level_up_points`, `stats` (`strength`, `agility`, `vitality`, `energy`,
`command`, without bonuses), `combo` (the Blade Knight's combo from Marlon),
`quests` (the seven legacy quests with `index`, `name` and `state`:
`active`, `complete`, `not_started`, `none`, or `unknown` for a value outside
these, as in the `quest` events), and `npc_quest`, the quest
dialog on screen (`quest`, `page`, `text`, `need_zen`, and its `answers`, each
with its `text` and `action`: `next` turns the page, `accept` takes the quest,
`complete` hands it in, `close` ends the talk) or `null`.

For native inventory transfer tests, `click-ui` accepts the optional boolean
`inventory_shift`. It holds the game's Shift polling state through the mouse
release and clears it on completion or cancellation. This is a control-socket
test facility; it does not synthesize text-field modifier chords or OS input.

Each `nearby` object carries `id`, `kind`, `name`, `position`, and a player,
monster or NPC also `alive`, `level` and `hp_percent`. `hp_percent` is a
percentage of full health (`100` is untouched) and is `null` when the server
has not told the client that object's health — a threshold test has to allow
for the null rather than read it as zero. A player, monster or NPC also
carries `pixel`, the window pixel `{x, y}` at the middle of the box the mouse
picks it by, or `null` while it is not drawn: `click-ui` there talks to an NPC
or targets a player the way a player's click does. The pixel is taken from the
last rendered frame, so it lags a moving object by a frame, and a window drawn
over it catches the click instead.

### Synthetic input

`hotkey` and `click-ui` deliver input to RmlUi first, then to the older
key/button readers only if the UI did not consume it. They do not move the OS
pointer or change window focus. A click rests on its pixel for two frames
before the press, as a real mouse does: an item grid only picks up an item it
saw hovered. Use screenshot pixels as `click-ui` coordinates: for
example, click the Menu button, then take another screenshot to inspect the
panel. The older `CInput` widgets still hit-test the OS cursor and cannot be
clicked remotely; use their keyboard navigation instead. Keys are
case-insensitive: letters, digits, `esc`, `enter`, `tab`, `space`, `backspace`,
`home`, `end`, `insert`, `delete`, `pageup`, `pagedown`, arrows,
`printscreen` and `f1`–`f12`.

Focus a text field with a supported UI click or keyboard navigation before
`type`. For example, `{"cmd":"type","text":"hello"}` inserts committed text;
`{"cmd":"type","text":"hello","enter":true}` delivers Return on a later rendered frame.
`text` must be 1–256 UTF-8 bytes with no NUL, ASCII control character or DEL;
`enter` must be boolean. A successful reply confirms delivery, **not** that a
field accepted the characters or a login succeeded. Its result reports only
byte count and whether Enter was requested, never the text (which may be a
password). No IME composition or key chords are synthesized.

`wheel` queues one SDL wheel event per rendered frame through the main loop's
ordinary event queue, so RmlUi and the legacy `MouseWheel` readers both see
device-like notches; with `x` and `y` it first queues a motion to that window
pixel, exactly like `hover`. It answers once the last notch has been consumed,
with `notches` and the pointer (`x`, `y`) it scrolled at.
`{"cmd":"wheel","notches":-3,"x":400,"y":300}` scrolls three notches towards
the user over the pixel (400, 300).

`drag` is one atomic injection routed like `click-ui`: the pointer arrives at
`from` and the button goes down, the pointer moves one straight-line step per
rendered frame for `steps` frames, and the button is released at `to`. It
answers after the release with `from`, `to`, `steps`, `button` and the final
pointer (`x`, `y`). `{"cmd":"drag","from":[100,200],"to":[300,200],"steps":8}`
drags ten frames in all. A drag that times out, is cancelled by a physical
press of the same button, loses its delivery target or outlives its scene
(`failed`) takes its press back without a release edge, like an abandoned
click: no button stays held and nothing is dropped behind the caller's back.

Positions of `wheel` and `drag` must lie inside the game window
(`bad_request` otherwise). Both share the single injection slot with
`hotkey`, `click-ui` and `type` (`busy`).

Only one injection runs at a time; another answers `busy`. Injection commands
are observational and do not interrupt an ongoing world act. They answer after
release processing on rendered frames. A stalled client times out and retracts
pending input rather than delivering it later. Cancellation is not a rollback:
text or effects already delivered before cancellation remain. An abandoned
held RmlUi click is cleared without activating the pressed element. Independent
physical input continues to work; a physical press of the same button cancels
an outstanding scripted click before that press is routed, returning a
`failed` response naming the physical press; a lost window/UI delivery target
instead reports `failed` with a target-disappeared message. UI hover can remain
at the injected pixel after a completed click or a cancelled click whose UI
press did not need clearing (a non-primary click or a primary click the UI did
not consume). A later physical or scripted motion updates it; the OS pointer
itself never moves.
A scripted key also uses the human's current physical modifiers for RmlUi
navigation; focused portable fields receive the key without those modifiers.

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
  Instrumented current builds additionally return `observability.version: 4`, with
  `messagebox_active` (native dialog stack, independent of manager visibility),
  `legacy_popup_active`, `generic_confirm_active`/`generic_menu_active`,
  `helper_active`, `input_focused` and `friend_open_allowed`.
  `input_focus_owner` is `"chatinputbox"` only for focus held by that exact
  registered panel's chat/whisper field; otherwise it is null (unidentified).
  Activity is boolean when observable and `null` when unavailable; never treat
  missing/null as inactive. `generic_dialogs_supported: false` explicitly marks
  a build without those RmlUi mechanisms; their activity fields stay null.
  Schema 3 additionally requires `crywolf_event_active` and
  `siegewarfare_child_active`: nullable booleans observed synchronously on the
  main thread. Both are null unless the world is ready and the exact system-owned
  instance is registered. CryWolf is active throughout its authoritative event
  map context, even with no dialog drawn. SiegeWarfare is active whenever its
  actual child exists, including outside the castle map; manager visibility and
  `IsCreated()` do not establish inactivity. Guarded replay requires both fields
  to be boolean false, whether the managers are visible or hidden. It never hides
  these managers or treats them as unconditionally passive.
  Schema 4 also reports `pointer: [window_x, window_y, logical_x, logical_y]` independently of quick-peer visibility when the world is ready (otherwise null). The first pair is SDL hover window pixels; the second is the legacy render cursor position after the screen-overlay transform. These units differ and may differ between clients using different transforms. A hover acknowledgement is not evidence that the measured cursor reached its requested position; re-observe and compare both representations. The complete pointer value participates in the opaque UI guard. The protected schema-3 old client exposes these four coordinates only via a valid explicit quick peer.
  Schema 4 also reports `input_idle` independently of quick-peer visibility:
  it uses the same physical key/mouse, pending SDL events, key edges and synthetic
  input predicate as the quick-peer guard. It is null outside a ready world;
  false or null never proves safe input. The protected original comparison
  binary still has schema 3 and supplies this idle predicate only through a
  valid explicit quick peer. Consumers must require the expected integer schema
  and reject missing, null or malformed safety fields before input. Forward the complete opaque `guard`
  unchanged; both fields participate in that snapshot, so event-state drift
  refuses before the normal synchronous callback. Rebuilding does not update an
  already-running client: reload verified builds separately before using schema 4.
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
  The world-scene response also includes `layout`: resolved workspace slot
  rectangles (`x`, `y`, `width`, `height`, `scale`) in window pixels, keyed by
  control-window name, plus `main_hud`. Closed/unslotted windows are omitted.
  These rectangles describe the slots before any saved drag offset and are
  diagnostic geometry, outside the opaque input guard. Inspect a screenshot
  and actual hit areas as well when verifying drawn placement.

- `ui list` on the character list answers that scene's own read-only report
  instead of the (stale) main-scene registry: `version: 1`,
  `scene: "character_list"`, `roster_received`, `selected_slot` (1-based or
  null), `server_message` (`visible`, `lines` 0..5 and their `texts`, both null
  when the stored count is impossible), `windows` (`character_select`,
  `character_make`, `message`, `system_menu`, `option`, `generic_confirm`,
  `generic_menu`: boolean, or null where the build has no such window),
  `input_focused`, `input_idle` (the quick-peer guard's idle predicate),
  `pointer: [window_x, window_y, logical_x, logical_y]`, plus `theme` and
  `ui_scale_percent`. It has no `guard`: nothing on that scene is guarded,
  and every other `ui` action keeps its world-only behaviour.
- `ui list` on the login scene (server list and login form) answers that
  scene's own read-only report the same way: `version: 1`, `scene: "login"`,
  `server_groups` (groups in the connect server's list), `login_form_ready`
  (the game server accepted the join), `windows` (`server_select`, `login`,
  `message`, `system_menu`, `option`, `credit`, `generic_confirm`,
  `generic_menu`: boolean or null), `login_form` (`account`: the account
  field's text, `password_empty`: whether the password field is empty — its
  text is never reported — and `focused_field`: `account`, `password` or
  null), `input_focused`, `input_idle`, `pointer`, `theme`, `ui_scale_percent`.
- `select-slot`, `server-list` and `select-server` exist because the login and
  character scenes read mouse buttons through `CInput`, which drops every
  button while the window has no keyboard focus: a parked, unfocused client
  cannot be clicked there. Each calls the code the click would have run.
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
| `stat` | `stat` (`life`, `mana`, `sd`, `ag`, `level`, `experience_gained` — the experience of one kill, and `damage_dealt` with it — the cumulative total is `state.experience`), `value`, `max` |
| `chat` | `sender`, `text`, `kind` (`public`, `whisper`, `party`, `guild`, `union`, `gens`, `gm`) |
| `drop` / `drop_gone` | `id` (the id `pickup` takes), `item`, `position` / `reason` |
| `map` | `map`, `map_name`, `position` |
| `scene` | `scene` |
| `view_enter` / `view_leave` | `object` |
| `quest` | `change`: `state` with the legacy `quest` and its new `state` (`active`, `complete`, `not_started`, `none`, `unknown`), or `reward` with the character's `name`, the `reward` (`level_up_points`, `second_class`, `points_per_level`, `combo`, `third_class`), its `amount` (none for the two class changes, whose new class is `class`) and the character's `class` afterwards; the client records the rewards of every player in view, so a script matches its own `name` |
| `party` | `change` (`invited` with the inviter's `name`; `list` with the leader's `name` after every change of the members; `left`; `result` with `result` — `failed`, `denied`, `full`, `user_left`, `other_party`, `left`, `opposing_gens`, `battle_zone`, `battle_zone_off` — when an invitation formed no party) |
| `trade` | `change` (`requested`, `opened`, `refused`, `unavailable`, `partner_confirm`, `closed`), `name` for a request or an opened trade, `state` (`checked`, `unchecked`, `reset`; `unknown` for a value outside the protocol) for the partner's button, `result` (`completed`, `cancelled`, `inventory_full`, `request_cancelled`, `reinforced_item`) when it closes; `refused` also on the asked side, when a window that forbids trading is open and the client says no by itself |
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
invitations and answers, quest states and rewards,
trade steps, logout) plus the scene and map watcher in `App/Control/ControlServer.cpp`.
When one of those functions is rewritten:

1. `rg -c 'App::Control::Events::' src/source/Network/Server/WSclient.cpp` —
   the count is 32; a lower one means a tap was dropped. Compare it against
   `git show upstream/main:…` when the number itself is in doubt: the count
   is a smoke test, the list above is the contract.
   The debug-ui mute adds `App::Control::Net::DropIncoming` at the top of
   `HandleIncomingPacket`.
2. Re-run the live checks that cover the dropped tap (a fight records `hit`,
   `killed` and `stat`; a pickup records `drop` and `drop_gone`; the `trade`
   in-game test records the trade steps, see [in-game-tests.md](in-game-tests.md)).

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

#### Explicit quick-command peer variant

`SOC-14.comparison-peer-target` opens ordinary target choices, never a choice itself.
No-target quick menus and physical Alt/right-click behavior are not covered. For
this route, send `ui list` with integer `peer_key` and string `peer_id` identifying
the intended live comparison character. Use its fresh opaque guard with
`guarded_show` or `guarded_hide`, `window: "quick_command"`, and the same two peer
fields. The version-1 `quick_peer` sub-contract is required; ordinary guarded
observations and existing unguarded commands retain their semantics. A list without
peer fields is suitable for other guarded panels, not quick commands.

The route resolves the current bounded world-array entry by both server key and
name. Server keys are not array indices. It refuses missing/ambiguous/self targets,
unsafe natural context, nonplayers, excluded subtypes, nonfinite/out-of-range
positions, unproven storage, pending/held/released/synthetic input and unsafe
placement. The existing pointer is not moved. It uses the natural caller's logical
coordinate transform and requires the complete menu to fit on screen. A refusal
near a screen edge is not permission to force visibility.

This binds semantic current peer identity, not continuous allocation lifetime:
same-peer reappearance with identical current conditions may be accepted. Changed
identity, index or guard prerequisites refuse. Opening leaves gameplay selection
unchanged; closing requires the identified current menu. Read `quick_peer` after
settling and around capture to verify `menu_visible`, `menu_matches`, name/index,
position and prerequisites, then inspect genuine full-frame content. The quick
settle action waits for two distinct completed UI-update/render frame boundaries,
not two dispatcher ticks or elapsed time. Same-poll and repeated same-frame ticks
cannot complete it. World/loading/registry/peer readiness is checked before every
deferred full snapshot; any snapshot drift stops without cleanup. Skipped UI
update/render frames do not advance settlement (the normal timeout still applies).
It does not
replace complete capture-time UI/modal checks, prevent future human input or make
Trade/Buy/Party/Follow/Duel safe. No command should be selected during comparison.

The adapter is source-side only until separately built, reviewed, ported and loaded
on both clients; a rebuild does not update a running client.

### Owned inert modal fixture (control builds only)

The `ui` actions `fixture_inspect`, `fixture_create`, and `fixture_retire` are a
bounded local observation fixture, not general dialog scripting. They never accept
caller content, callbacks or invitation/transaction actions. This RmlUi-side API
is source-tested; loading it and using it live requires separate authorization.

1. Send `{"cmd":"ui","action":"fixture_inspect","token":""}`. Save the returned
   `guard` string verbatim. Inspect is read-only and reports version 1, current `ui`
   observations, exact manager registration, nullable token validity/activity/queue/
   click state, and read-only `input_idle`. Unavailable owners remain null.
2. Send `ui` / `fixture_create` with `token:""`, `fixture_version:1`,
   `fixture_opt_in:true`, `nonce` (1–64 ASCII letters/digits/underscore/hyphen), and
   that `guard`. It opens the normal generic menu with the fixed title
   **UI comparison fixture**, text **Local observation test. No game action.**,
   and an inert **Close** button. It returns the unique process-local `token`.
3. Inspect again with that token. Retire using `ui` / `fixture_retire`, the same
   token, `fixture_version:1`, `fixture_opt_in:true`, and the fresh returned guard.
   Successful retirement is synchronous. Re-inspect; the old token is invalid.

Both mutations require known world/registration/modal state, unmuted networking,
no unrelated ordinary panel, no pending control act, and no queued/held/edge or
synthetic input. Exact full guard equality is necessary but insufficient: fresh
safety predicates and exact private active-config ownership are also checked.
No event queue is drained. A queued menu, pending click, replacement, stale token
or missing manager refuses without cleanup. A refusal is not permission to dismiss
anything through a different command.

The token is privately associated only with the fixed callback-free active config;
it cannot be attached to caller configs or queued menus. Ordinary close, Release,
replacement, queue promotion and object destruction invalidate it, including when
Release retains stored content or a later object reuses the same address. Tokens
use a fixture-only non-reused process sequence (overflow refuses), not a global UI
epoch. Singleton/purpose/caption identity never grants retirement. Ordinary dialog
callers are unchanged; token storage/hooks do not exist with control disabled.

Headless tests exercise real logical creation/registration/observation/retirement,
not document rendering or live input. Unknown live observations must arise naturally;
there is no API to hide/unregister owners or overwrite observations. No native-side
fixture command is supplied by this RmlUi implementation.

`talk` accepts a visible NPC `target` (id or name) within three tiles of a living
character. It sends the ordinary TalkToNpcRequest; `sent: true` confirms dispatch,
not acceptance. Wait for the real NPC response/window before item transactions.
This allows native shop/vault/mixing acceptance without fabricated incoming packets.
