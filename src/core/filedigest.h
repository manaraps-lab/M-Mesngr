// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
//
// Incremental SHA-256 used to verify that a transferred file arrived intact.
// Implemented with OpenSSL EVP so it uses hardware acceleration when present.
#pragma once

#include <QByteArray>
#include <QString>

struct evp_md_ctx_st;

class FileDigest {
public:
  static constexpr int kSize = 32;  // SHA-256 digest length in bytes

  FileDigest();
  ~FileDigest();
  FileDigest(const FileDigest&) = delete;
  FileDigest& operator=(const FileDigest&) = delete;

  // Starts a new digest, discarding anything fed so far.
  void reset();
  void update(const char* data, qint64 length);
  // Returns the 32 byte digest and resets the object for reuse.
  QByteArray finish();

  // Convenience for tests and tools: digest of a whole file. Returns an empty
  // array (and sets *ok to false) when the file cannot be read.
  static QByteArray ofFile(const QString& filePath, bool* ok = nullptr);

private:
  evp_md_ctx_st* ctx;
};
