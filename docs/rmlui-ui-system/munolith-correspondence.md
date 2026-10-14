# Munolith correspondence

The Munolith theme supplies a coordinated floating Friends and mailbox window,
letter reader and composer, and chat room. Default sizes are 420 by 340, 460 by
300, and 400 by 320 dp respectively. Lists and letter bodies scroll within their
panels; native floating placement, resize/minimize controls and instance models
remain responsible for window state.

The documents retain the original event bindings and field IDs. Letter portraits
continue to use the native render target, help, rotation and pose controls.
Messaging and server delivery are unchanged. Verification uses injected contacts
and letters and unsent text in an isolated client; it does not send mail or chat.

Chat uses automatic overflow scrollbars and one labelled invitation drawer, which
replaces the participant column while open. Sender and message wrap together.
Invitation resizing waits for the data views to apply visibility, and closing
undoes the actual expansion so viewport caps do not cause repeated shrinking.
Initial failed connections show the native offline title and lock message entry.
Populated native verification uses an owned loopback chat fixture; synthetic
incoming text does not imply delivery through a production chat server.
