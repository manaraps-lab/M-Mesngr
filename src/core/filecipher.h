// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
//
// Authenticated encryption of file content in transit (AES-256-GCM).
//
// One random key is created per file and travels inside the file offer, which
// is itself encrypted with the receiver's public key like every other message.
// The content is cut into records:
//
//     [ 4 bytes big-endian n ][ n bytes ciphertext ][ 16 bytes tag ]
//
// Each record's nonce is a counter (4 zero bytes + 8 byte big-endian record
// number). The key is never reused for another file, so a nonce never repeats,
// and a record that is dropped, repeated or reordered fails authentication.
//
// What this does and does not give: it hides the content from someone who only
// listens to the network. It is no stronger than the key exchange of the
// messages that carry the key; the public keys are not authenticated, so an
// attacker who can actively alter traffic during connection set-up is not
// stopped.
#pragma once

#include <QByteArray>

struct evp_cipher_ctx_st;

class FileCipher {
public:
  static constexpr int kKeySize = 32;
  static constexpr int kTagSize = 16;
  static constexpr int kHeaderSize = 4;
  //  Largest plaintext record accepted when decrypting.
  static constexpr int kMaxRecord = 1024 * 1024;

  explicit FileCipher(const QByteArray& key);

  //  32 random bytes from the OpenSSL CSPRNG; empty on failure.
  static QByteArray randomKey();

  bool isValid() const { return key.size() == kKeySize; }

  //  Encrypts one record. Returns header + ciphertext + tag, or an empty array
  //  on failure (or when len is not in 1..kMaxRecord).
  QByteArray seal(const char* data, qint64 len);

  //  Decrypts one record whose ciphertext is `data[0..len)` and whose tag is
  //  `tag[0..kTagSize)`. Returns false when authentication fails.
  bool open(const char* data, qint64 len, const char* tag, QByteArray& plain);

private:
  QByteArray key;
  quint64 counter = 0;
};
