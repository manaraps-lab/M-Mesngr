// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
#include "filedigest.h"

#include <QFile>
#include <openssl/evp.h>

FileDigest::FileDigest() : ctx(EVP_MD_CTX_new()) {
  reset();
}

FileDigest::~FileDigest() {
  EVP_MD_CTX_free(ctx);
}

void FileDigest::reset() {
  if(ctx == nullptr)
    return;
  EVP_MD_CTX_reset(ctx);
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
}

void FileDigest::update(const char* data, qint64 length) {
  if(ctx == nullptr || length <= 0)
    return;
  EVP_DigestUpdate(ctx, data, static_cast<size_t>(length));
}

QByteArray FileDigest::finish() {
  QByteArray out(kSize, 0);
  unsigned int outLen = 0;
  if(ctx == nullptr || EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen) != 1 ||
     outLen != static_cast<unsigned int>(kSize)) {
    out.clear();
  }
  reset();
  return out;
}

QByteArray FileDigest::ofFile(const QString& filePath, bool* ok) {
  if(ok)
    *ok = false;
  QFile file(filePath);
  if(!file.open(QIODevice::ReadOnly))
    return QByteArray();

  FileDigest digest;
  QByteArray buffer(1024 * 1024, 0);
  for(;;) {
    const qint64 n = file.read(buffer.data(), buffer.size());
    if(n < 0)
      return QByteArray();
    if(n == 0)
      break;
    digest.update(buffer.constData(), n);
  }
  if(ok)
    *ok = true;
  return digest.finish();
}
