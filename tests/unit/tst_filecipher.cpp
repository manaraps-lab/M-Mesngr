// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
#include <QtTest>

#include "core/filecipher.h"

class TstFileCipher : public QObject {
  Q_OBJECT
private slots:
  void roundTrip_data();
  void roundTrip();
  void tamperedRecordFails();
  void reorderedOrRepeatedRecordFails();
  void wrongKeyFails();
  void badLengthsAreRefused();
  void knownAnswer();
};

static bool openRecord(FileCipher& c, const QByteArray& rec, QByteArray& plain) {
  const int n = int((quint8(rec[0]) << 24) | (quint8(rec[1]) << 16) | (quint8(rec[2]) << 8) | quint8(rec[3]));
  return c.open(rec.constData() + 4, n, rec.constData() + 4 + n, plain);
}

void TstFileCipher::roundTrip_data() {
  QTest::addColumn<int>("size");
  for(int s : {1, 2, 15, 16, 17, 4096, 256 * 1024, FileCipher::kMaxRecord})
    QTest::newRow(qPrintable(QString::number(s))) << s;
}

void TstFileCipher::roundTrip() {
  QFETCH(int, size);
  const QByteArray key = FileCipher::randomKey();
  QCOMPARE(key.size(), FileCipher::kKeySize);
  QByteArray data(size, 0);
  for(int i = 0; i < size; ++i) data[i] = char(i * 31 + 7);
  FileCipher enc(key), dec(key);
  for(int round = 0; round < 3; ++round) {  // several records: counters stay in step
    const QByteArray rec = enc.seal(data.constData(), data.size());
    QCOMPARE(rec.size(), 4 + size + FileCipher::kTagSize);
    QVERIFY(!rec.mid(4, size).startsWith(data.left(16)) || size < 16);
    QByteArray plain;
    QVERIFY(openRecord(dec, rec, plain));
    QCOMPARE(plain, data);
  }
}

void TstFileCipher::tamperedRecordFails() {
  const QByteArray key = FileCipher::randomKey();
  const QByteArray data(1000, 'q');
  FileCipher enc(key);
  const QByteArray rec = enc.seal(data.constData(), data.size());
  for(int pos : {4, 500, 4 + 1000 - 1, 4 + 1000, 4 + 1000 + 15}) {  // ciphertext and tag bytes
    QByteArray bad = rec;
    bad[pos] = char(bad[pos] ^ 0x80);
    FileCipher dec(key);
    QByteArray plain;
    QVERIFY2(!openRecord(dec, bad, plain), qPrintable(QString::number(pos)));
    QVERIFY(plain.isEmpty());
  }
}

void TstFileCipher::reorderedOrRepeatedRecordFails() {
  const QByteArray key = FileCipher::randomKey();
  FileCipher enc(key);
  const QByteArray a = enc.seal("first record", 12);
  const QByteArray b = enc.seal("second record", 13);
  QByteArray plain;
  {  // swapped
    FileCipher dec(key);
    QVERIFY(!openRecord(dec, b, plain));
  }
  {  // first record delivered twice
    FileCipher dec(key);
    QVERIFY(openRecord(dec, a, plain));
    QVERIFY(!openRecord(dec, a, plain));
  }
  {  // first record dropped
    FileCipher dec(key);
    QVERIFY(!openRecord(dec, b, plain));
  }
}

void TstFileCipher::wrongKeyFails() {
  FileCipher enc(FileCipher::randomKey());
  const QByteArray rec = enc.seal("hello world", 11);
  FileCipher dec(FileCipher::randomKey());
  QByteArray plain;
  QVERIFY(!openRecord(dec, rec, plain));
}

void TstFileCipher::badLengthsAreRefused() {
  FileCipher c(FileCipher::randomKey());
  QVERIFY(c.seal("x", 0).isEmpty());
  QVERIFY(c.seal("x", -1).isEmpty());
  QVERIFY(c.seal(QByteArray(FileCipher::kMaxRecord + 1, 'x').constData(), FileCipher::kMaxRecord + 1).isEmpty());
  FileCipher bad(QByteArray(5, 'k'));  // wrong key size
  QVERIFY(!bad.isValid());
  QVERIFY(bad.seal("x", 1).isEmpty());
}

// Independent check against a published AES-256-GCM vector (NIST GCM test case
// 14: all-zero key, all-zero 96-bit IV, 16 zero bytes of plaintext): the record
// layer must produce exactly that ciphertext and tag for counter 0.
void TstFileCipher::knownAnswer() {
  FileCipher c(QByteArray(32, 0));
  const QByteArray zeros(16, 0);
  const QByteArray rec = c.seal(zeros.constData(), 16);
  QCOMPARE(rec.mid(4, 16).toHex(), QByteArray("cea7403d4d606b6e074ec5d3baf39d18"));
  QCOMPARE(rec.mid(4 + 16, 16).toHex(), QByteArray("d0d1c8a799996bf0265b98b5d48ab919"));
}

QTEST_GUILESS_MAIN(TstFileCipher)
#include "tst_filecipher.moc"
