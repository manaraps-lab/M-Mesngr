# Security notes (MANARA changes)

Source of the findings: docs/PHASE1_SOURCE_REVIEW.md (S1–S8).

## Fixed in this patch
* **S1 path traversal on receive.** File names, folder names and relative paths
  received from a peer are reduced to safe components (`core/pathsafety.*`):
  `..`, absolute paths, drive letters, `\` and `/`, reserved Windows device
  names (CON, NUL, COM1…), trailing dots/spaces, alternate-stream colons and
  control characters are removed; length and depth are bounded. Folder entries
  with an unsafe path are declined; the resolved path must stay inside the folder
  (`isInside`). 28 unit tests.
* **Avatar file name** built from the peer's user id is sanitised too.
* **Unbounded message frame.** `MsgStream` drops a connection announcing a frame
  over 16 MiB and reassembles split headers/frames correctly.
* **Socket and memory leaks.** Unknown or unmatched incoming sockets are closed;
  per-transfer buffers/files are freed.
* **Corrupted/partial files** are deleted and never reported as received.

## Not fixed (still true)
* Discovery (UDP multicast) is plaintext. Anyone on the LAN can see user names
  and presence.
* **File contents are not encrypted.** SHA-256 detects accidental corruption only;
  an attacker who can modify the stream can also modify the hash.
* Text encryption is RSA-2048/OAEP-SHA1 + AES-256-CBC with EVP_BytesToKey and
  no MAC (upstream design). Not changed here.
* The file-transfer socket is matched by file id + user id only; the peer
  address is **not** enforced (multi-homed hosts would break). The 32-char id is
  unguessable in practice but is not an authentication token.
* No "Private network only" enforcement inside the app; the installer adds
  firewall rules for the **Private** profile only (setup/win32/setup.nsi).

Nothing here is a security audit; it is a review and targeted fixes.
