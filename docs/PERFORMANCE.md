# File transfer: what was changed, tested and measured

Principle: nothing here is claimed unless it was run. Everything below was run
on a **cloud Linux VM (2 vCPU Intel Xeon 2.1 GHz, Qt 6.8.3 minimal build, GCC
13, OpenSSL 3.0.13)** over **loopback** (127.0.0.1, sender and receiver in one
process and one event loop). Loopback measures the software path (disk, copies,
hashing, event loop). It is **not** a LAN measurement and must not be quoted as
LAN speed. A real measurement on two Windows PCs and a real switch has **not**
been done yet (see "Not tested").

## Changes in the engine (`src/network/netstreamer.*`)

| Area | Before (upstream 654d1f09) | Now |
|---|---|---|
| Chunk size | 64 KiB − 1, one write per `bytesWritten` | 256 KiB, write queue kept ≤ 1 MiB (disk and network overlap) |
| Socket buffers | OS default | 1 MiB requested for send/receive |
| Memory | chunk buffer and `QFile` never freed per transfer | `QByteArray` buffer, `QFile` owned by the transfer object |
| Progress shown | file read position (includes data only queued in the socket) | bytes actually handed to the OS (`sent − bytesToWrite`) |
| Integrity | none | SHA-256 trailer, verified by the receiver (below) |
| Failure | partial file could remain after a disconnect | partial/corrupt file is deleted, reported as error, never as complete |
| Zero-byte file | never completed | completes |
| `MsgStream` frames | no size limit; a split 4-byte header was mis-parsed | 16 MiB cap (connection dropped above it); frames reassembled from any split |
| Unknown / unmatched sockets | kept open forever | closed |

### SHA-256 negotiation (compatible with old peers)
The sender adds `<filehash>sha256</filehash>` to the file offer. A new receiver
echoes it in its accept message; only then does the sender append the 32-byte
digest after the file bytes and the receiver verify it. Against an old peer the
tag is ignored/absent and the wire format is exactly the old one (tested:
`legacyPeerStillWorks`). The file bytes are **not encrypted**; the hash detects
corruption and accidental damage, it does **not** protect against an attacker on
the network who can alter both data and the hash (see SECURITY.md).

## Tests (all passing here)
`tst_transfer` (12 cases): intact transfer for 0 B, 1 B, exactly one chunk,
chunk+1, 5.3 MiB (digest on); legacy peer (digest off, 0 B / 777 B / 3 MiB);
**one flipped bit in transit → receiver reports error and deletes the file**;
**connection cut mid-file → error, no partial file**; frame reassembly with
3-byte fragments; oversized frame closes the connection.
`tst_pathsafety` (28), `tst_filedigest` (9: NIST vectors for "" and "abc",
chunking independence, file digest, speed meter).

## Loopback measurements (512 MiB file, MiB/s, same VM, runs alternated)
| Engine | Runs |
|---|---|
| Upstream 654d1f09 | 798, 1078, 1174, then 1333, 1506 |
| New, no digest | 979, 1309, 1169 |
| New, SHA-256 on | 391, 304, 319, then 372, 453 |

Honest reading:
* The spread between runs (±25 %) is larger than any difference between upstream
  and the new engine without digest. **No speed gain is claimed from the chunk
  change.** Upstream already moved >0.8 GiB/s on loopback, i.e. the old engine was
  not the limit on a gigabit LAN (≈ 118 MiB/s) either; on real networks the limit
  is the link, the disk and Wi-Fi quality.
* SHA-256 costs throughput here because *both* ends hash in one process on 2
  cores. In real use each PC hashes its own side in parallel. Raw OpenSSL SHA-256
  on this VM: ≈ 1.6–1.8 GB/s (`openssl speed`), far above gigabit speed, but it
  has **not** been measured on a real transfer between two PCs, nor on 2.5/10 GbE.
* The hash is computed on the GUI thread per 256 KiB chunk (≈ 0.15 ms each at the
  rate above). Not measured for UI smoothness on Windows.

## Live speed and time left
`TransferMeter` (src/core/transfermeter.h) replaces "average since start,
averaged with the previous value": it is an exponentially smoothed rate (time
constant 2 s) over progress samples and reports "Calculating time" until two
samples exist. Unit-tested (steady rate, rate drop, rewind, unknown). Wired into
`lmcTransferWindow::updateProgress` — **that GUI code was not compiled here**
(no QtGui/QtWidgets in the local build); CI must build it.

## Not tested / not claimed
* Windows build, GUI, installer, firewall rules, the new QSS/theme visually.
* Real LAN / Wi-Fi throughput, large (>4 GiB) files, many simultaneous transfers.
* Folder transfers with the new hash path (same code path, no dedicated test).
* Behaviour of mixed old/new peers beyond the byte-level compatibility test.
* `tst_theme` against `Manara.json` (theme parser needs QtGui).

## Reproduce
```
cmake -B build -DLMC_BUILD_BENCH=ON && cmake --build build --target transferbench
./build/transferbench 512 3            # without digest
./build/transferbench 512 3 digest     # with SHA-256
ctest --test-dir build -R "transfer|pathsafety|filedigest"
```
