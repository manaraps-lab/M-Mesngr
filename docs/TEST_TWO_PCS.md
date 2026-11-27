# Two-PC test checklist (MANARA Messenger)

Setup: install the same build on both PCs, same LAN, Windows network profile **Private**.

1. Each PC lists the other within ~30 s.
2. Chat: send text both ways; messages appear in the side pane; first incoming message opens its chat; the orange badge counts messages from other chats.
3. File (small, ~1 MB) A->B: B gets a notification and a file card; Accept; progress, speed and ETA update; compare `Get-FileHash` on both sides (must match).
4. File (large, 1-5 GB): record time and size, report MB/s. Cancel half-way on a second run: no partial file may remain.
5. Decline a file: sender shows declined; nothing is written.
6. Folder transfer.
7. Name collision: send the same file twice; the second must not overwrite the first.
8. Firewall: first run asks (or installer rule exists) for the Private profile only.
9. Optional hardening: set `Security/EnforcePeerAddress=true` on the receiver and repeat step 3; report any rejection (log shows "Rejecting file connection").
10. Encryption: with Wireshark on one PC, a transferred text file must not show its contents in the capture.

Report: Windows version, NIC type (Wi-Fi/Ethernet), measured MB/s, and any message in the log.
