# Command completion and state ownership

Native controls and the live editor control adapter share one document owner.
Successful authored edits enter the same Undo/Redo history. Revision checks are
performed when the editor dispatches a request, not when a client sends it.
A native preview or modal draft can temporarily refuse external mutation.

| Family | Owner, acceptance and completion |
| --- | --- |
| Objects, materials, camera, lighting, atmosphere | Stable object/attachment identity and current document revision select the authored target. Native panels and control adapters call the retained document commands. Invalid candidates preserve state and history. Gesture previews are not committed edits. |
| Animation, paths, motion plans, sequences and events | Document/timeline commands own authored history. View/playhead sampling and evaluated runtime snapshots are separate from authored edits. Preview/cancel must not become a new history entry. |
| Undo/Redo | The same document history serves native and control routes. A stale or malformed command is refused before history changes; retrying an old revision cannot silently repeat Undo. Read current state after an uncertain response. |
| Open, Create, Save As and Save | Scene lifecycle owner validates and reserves the destination before switching. Dirty-scene and active-interaction guards apply. Publication and directory-sync outcomes follow the persistence contract; a committed-but-uncertain save is not an unapplied edit to replay blindly. |
| Native text, gizmos and modal panels | UI owner holds temporary draft/preview state. Cancel, lost scope or changed revision discard the preview. A completed gesture enters one document transaction. |
| Local frame rendering | Renderer captures admitted working-scene inputs and owns the job. Start acknowledges admission; progress, cancellation and verified result identify completion separately. Rendering does not enter authored Undo. Read the same job after an uncertain reply; a new request ID is new work. |
| Offline render/material-preview/asset tools | Each process owns its request and output. They are not attached to an open native document. Job-runner publication/recovery uses its own retained identities. |

Method and optional scene-path strings must contain their complete JSON value;
embedded NUL bytes cannot alias another command or target. The legacy optional
scene-path field remains compatible, while monotonic document revisions prevent
old document commands from becoming valid after a switch. Render cancellation
uses its admitted job identity and current revision, independently of authored
history and interaction guards.

The reusable L2 tests add malformed/stale/foreign command refusal and native/control
history parity to the runtime scene-editor group. Native object, render and scene
lifecycle integration fixtures exercise the actual control protocol and fresh
process reopen. Source qualification is separate from installed package or physical
renderer accuracy qualification.
