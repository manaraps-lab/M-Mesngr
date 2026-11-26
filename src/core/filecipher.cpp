// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
#include "filecipher.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <memory>

namespace {

struct CtxDeleter {
  void operator()(EVP_CIPHER_CTX* c) const { EVP_CIPHER_CTX_free(c); }
};
using CtxPtr = std::unique_ptr<EVP_CIPHER_CTX, CtxDeleter>;

void makeNonce(quint64 counter, unsigned char nonce[12]) {
  nonce[0] = nonce[1] = nonce[2] = nonce[3] = 0;
  for(int i = 0; i < 8; ++i)
    nonce[4 + i] = static_cast<unsigned char>((counter >> (56 - 8 * i)) & 0xFF);
}

}  // namespace

FileCipher::FileCipher(const QByteArray& k) : key(k) {}

QByteArray FileCipher::randomKey() {
  QByteArray k(kKeySize, 0);
  if(RAND_bytes(reinterpret_cast<unsigned char*>(k.data()), kKeySize) != 1)
    return QByteArray();
  return k;
}

QByteArray FileCipher::seal(const char* data, qint64 len) {
  if(!isValid() || len < 1 || len > kMaxRecord)
    return QByteArray();

  unsigned char nonce[12];
  makeNonce(counter, nonce);

  CtxPtr ctx(EVP_CIPHER_CTX_new());
  if(!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
     EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
     EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()), nonce) != 1)
    return QByteArray();

  QByteArray out(kHeaderSize + static_cast<int>(len) + kTagSize, 0);
  unsigned char* p = reinterpret_cast<unsigned char*>(out.data());
  p[0] = static_cast<unsigned char>((len >> 24) & 0xFF);
  p[1] = static_cast<unsigned char>((len >> 16) & 0xFF);
  p[2] = static_cast<unsigned char>((len >> 8) & 0xFF);
  p[3] = static_cast<unsigned char>(len & 0xFF);

  int outLen = 0;
  if(EVP_EncryptUpdate(ctx.get(), p + kHeaderSize, &outLen, reinterpret_cast<const unsigned char*>(data),
                       static_cast<int>(len)) != 1 ||
     outLen != static_cast<int>(len))
    return QByteArray();
  int finLen = 0;
  if(EVP_EncryptFinal_ex(ctx.get(), p + kHeaderSize + outLen, &finLen) != 1)
    return QByteArray();
  if(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, kTagSize, p + kHeaderSize + len) != 1)
    return QByteArray();

  ++counter;
  return out;
}

bool FileCipher::open(const char* data, qint64 len, const char* tag, QByteArray& plain) {
  plain.clear();
  if(!isValid() || len < 1 || len > kMaxRecord)
    return false;

  unsigned char nonce[12];
  makeNonce(counter, nonce);

  CtxPtr ctx(EVP_CIPHER_CTX_new());
  if(!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
     EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1 ||
     EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const unsigned char*>(key.constData()), nonce) != 1)
    return false;

  QByteArray out(static_cast<int>(len), 0);
  int outLen = 0;
  if(EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(out.data()), &outLen,
                       reinterpret_cast<const unsigned char*>(data), static_cast<int>(len)) != 1)
    return false;
  // The tag is checked in Final; copy because the API takes a non-const pointer.
  unsigned char tagCopy[kTagSize];
  for(int i = 0; i < kTagSize; ++i)
    tagCopy[i] = static_cast<unsigned char>(tag[i]);
  if(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, kTagSize, tagCopy) != 1)
    return false;
  int finLen = 0;
  if(EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(out.data()) + outLen, &finLen) != 1)
    return false;

  ++counter;
  plain = out;
  return true;
}
