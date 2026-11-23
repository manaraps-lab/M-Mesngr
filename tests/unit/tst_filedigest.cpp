// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
#include <QtTest>
#include <QTemporaryFile>

#include "core/filedigest.h"
#include "core/transfermeter.h"

class TstFileDigest : public QObject {
  Q_OBJECT
private slots:
  void knownVectors();
  void chunkingDoesNotMatter();
  void fileDigest();
  void missingFile();
  void meterSteadyRate();
  void meterFollowsChange();
  void meterUnknownAndRewind();
};

static QByteArray hex(const QByteArray& b) { return b.toHex(); }

void TstFileDigest::knownVectors() {
  FileDigest d;
  QCOMPARE(hex(d.finish()), QByteArray("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  d.update("abc", 3);
  QCOMPARE(hex(d.finish()), QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

void TstFileDigest::chunkingDoesNotMatter() {
  QByteArray data(3 * 1024 * 1024 + 17, 0);
  for(int i = 0; i < data.size(); ++i) data[i] = char((i * 31) ^ (i >> 7));
  FileDigest a, b;
  a.update(data.constData(), data.size());
  for(qint64 off = 0; off < data.size(); off += 65535)
    b.update(data.constData() + off, qMin<qint64>(65535, data.size() - off));
  QCOMPARE(a.finish(), b.finish());
}

void TstFileDigest::fileDigest() {
  QTemporaryFile f;
  QVERIFY(f.open());
  f.write("abc");
  f.flush();
  bool ok = false;
  QCOMPARE(hex(FileDigest::ofFile(f.fileName(), &ok)),
           QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  QVERIFY(ok);
}

void TstFileDigest::missingFile() {
  bool ok = true;
  QVERIFY(FileDigest::ofFile("/nonexistent/zzz", &ok).isEmpty());
  QVERIFY(!ok);
}

void TstFileDigest::meterSteadyRate() {
  TransferMeter m;
  for(int i = 0; i <= 20; ++i) m.update(int64_t(i) * 1000000, int64_t(i) * 1000);  // 1 MB/s
  QVERIFY(qAbs(m.bytesPerSecond() - 1000000) < 1000);
  QCOMPARE(m.secondsLeft(10000000), int64_t(10));
}

void TstFileDigest::meterFollowsChange() {
  TransferMeter m;
  int64_t pos = 0, t = 0;
  for(int i = 0; i < 20; ++i) { t += 1000; pos += 10000000; m.update(pos, t); }  // 10 MB/s
  for(int i = 0; i < 10; ++i) { t += 1000; pos += 1000000; m.update(pos, t); }   // drops to 1 MB/s
  QVERIFY(m.bytesPerSecond() < 2000000);
  QVERIFY(m.bytesPerSecond() >= 1000000);
}

void TstFileDigest::meterUnknownAndRewind() {
  TransferMeter m;
  QCOMPARE(m.secondsLeft(100), int64_t(-1));
  m.update(0, 0);
  QCOMPARE(m.secondsLeft(100), int64_t(-1));
  m.update(1000, 1000);
  QVERIFY(m.secondsLeft(1000) > 0);
  m.update(10, 2000);  // rewind
  QCOMPARE(m.secondsLeft(100), int64_t(-1));
}

QTEST_APPLESS_MAIN(TstFileDigest)
#include "tst_filedigest.moc"
