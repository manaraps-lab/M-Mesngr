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
* **File encryption (new, negotiated per transfer).** When both sides support it,
  file bytes are sent as AES-256-GCM records with a random per-file key. The key
  travels only inside the already RSA+AES-encrypted offer message. This protects
  against passive eavesdropping and detects tampering of the stream. It does
  **not** stop an active man-in-the-middle during key exchange: public keys are
  not authenticated. Against an older peer the transfer falls back to plain
  bytes + SHA-256 (corruption detection only). Verified on Linux loopback and
  mock peers only; not yet on two Windows PCs.
* Text encryption is RSA-2048/OAEP-SHA1 + AES-256-CBC with EVP_BytesToKey and
  no MAC (upstream design). Not changed here.
* The file-transfer socket is matched by file id + user id. An **optional** check
  (`Security/EnforcePeerAddress=true` in the settings file, default **off**) also
  requires the connecting address to equal the address the peer was discovered
  at. It is off by default because a peer with several network cards may connect
  from another address. Not yet tested on real networks.
* No "Private network only" enforcement inside the app; the installer adds
  firewall rules for the **Private** profile only (setup/win32/setup.nsi).

Nothing here is a security audit; it is a review and targeted fixes.
