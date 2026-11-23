// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
// End-to-end loopback tests of FileSender/FileReceiver and MsgStream framing.
#include <QtTest>
#include <QTemporaryDir>

#include "core/filedigest.h"
#include "network/netstreamer.h"
#include "tools/loopback.h"

struct Outcome {
  FileOp op = FO_Max;
  bool done = false;
};

class TstTransfer : public QObject {
  Q_OBJECT
private:
  QTemporaryDir dir;
  QString makeFile(const QString& name, qint64 size);
  // Runs one transfer; returns {sender outcome, receiver outcome}.
  QPair<Outcome, Outcome> run(const QString& src, const QString& dst, bool senderDigest, bool receiverDigest,
                              qint64 flipAt = -1, qint64 cutAt = -1);
private slots:
  void initTestCase() { QVERIFY(dir.isValid()); }
  void withDigestArrivesIntact_data();
  void withDigestArrivesIntact();
  void legacyPeerStillWorks();
  void corruptionIsDetected();
  void truncationIsAnError();
  void msgStreamSplitHeader();
  void msgStreamRejectsHugeFrame();
};

QString TstTransfer::makeFile(const QString& name, qint64 size) {
  QString path = dir.filePath(name);
  QFile f(path);
  f.open(QIODevice::WriteOnly);
  QByteArray block(65536, 0);
  quint32 x = 12345u + quint32(size);
  qint64 left = size;
  while(left > 0) {
    for(int i = 0; i < block.size(); ++i) { x = x * 1664525u + 1013904223u; block[i] = char(x >> 24); }
    qint64 n = qMin<qint64>(left, block.size());
    f.write(block.constData(), n);
    left -= n;
  }
  return path;
}

QPair<Outcome, Outcome> TstTransfer::run(const QString& src, const QString& dst, bool senderDigest,
                                         bool receiverDigest, qint64 flipAt, qint64 cutAt) {
  const QString id(32, 'a');
  const qint64 size = QFileInfo(src).size();
  LoopbackServer server;
  QScopedPointer<TamperProxy> proxy;
  quint16 port = server.port();
  if(flipAt >= 0 || cutAt >= 0) {
    proxy.reset(new TamperProxy(server.port(), flipAt, cutAt));
    port = proxy->port();
  }

  FileReceiver receiver(id, "alice", dst, "x", size, "127.0.0.1", port, FT_Normal);
  receiver.setDigestEnabled(receiverDigest);
  server.setReceiver(&receiver);
  FileSender sender(id, "bob", "alice", src, "x", size, "127.0.0.1", port, FT_Normal);
  sender.setDigestEnabled(senderDigest);

  Outcome rx, tx;
  connect(&receiver, &FileReceiver::progressUpdated, [&](FileMode, FileOp op, FileType, QString*, QString*, QString*) {
    if(op == FO_Complete || op == FO_Error) { rx.op = op; rx.done = true; }
  });
  connect(&sender, &FileSender::progressUpdated, [&](FileMode, FileOp op, FileType, QString*, QString*, QString*) {
    if(op == FO_Complete || op == FO_Error) { tx.op = op; tx.done = true; }
  });
  sender.init();
  QTest::qWait(10);
  QElapsedTimer t; t.start();
  while((!rx.done || !tx.done) && t.elapsed() < 30000) QTest::qWait(5);
  // give a failing side time to report if the other already finished
  QTest::qWait(200);
  return qMakePair(tx, rx);
}

void TstTransfer::withDigestArrivesIntact_data() {
  QTest::addColumn<qint64>("size");
  QTest::newRow("empty") << qint64(0);
  QTest::newRow("1 byte") << qint64(1);
  QTest::newRow("one chunk exact") << qint64(256 * 1024);
  QTest::newRow("chunk+1") << qint64(256 * 1024 + 1);
  QTest::newRow("odd 5.3MB") << qint64(5 * 1024 * 1024 + 333);
}

void TstTransfer::withDigestArrivesIntact() {
  QFETCH(qint64, size);
  QString src = makeFile("src.bin", size), dst = dir.filePath("dst.bin");
  QFile::remove(dst);
  auto r = run(src, dst, true, true);
  QVERIFY(r.second.done);
  QCOMPARE(int(r.second.op), int(FO_Complete));
  QVERIFY(r.first.done);
  QCOMPARE(int(r.first.op), int(FO_Complete));
  QCOMPARE(QFileInfo(dst).size(), size);
  QCOMPARE(FileDigest::ofFile(dst), FileDigest::ofFile(src));
}

void TstTransfer::legacyPeerStillWorks() {
  for(qint64 size : {qint64(0), qint64(777), qint64(3 * 1024 * 1024 + 5)}) {
    QString src = makeFile("src2.bin", size), dst = dir.filePath("dst2.bin");
    QFile::remove(dst);
    auto r = run(src, dst, false, false);
    QVERIFY2(r.second.done && r.second.op == FO_Complete, qPrintable(QString::number(size)));
    QCOMPARE(FileDigest::ofFile(dst), FileDigest::ofFile(src));
  }
}

void TstTransfer::corruptionIsDetected() {
  QString src = makeFile("src3.bin", 2 * 1024 * 1024), dst = dir.filePath("dst3.bin");
  QFile::remove(dst);
  auto r = run(src, dst, true, true, /*flipAt*/ 1000000);
  QVERIFY(r.second.done);
  QCOMPARE(int(r.second.op), int(FO_Error));   // never reported complete
  QVERIFY2(!QFile::exists(dst), "corrupt file must be deleted");
}

void TstTransfer::truncationIsAnError() {
  QString src = makeFile("src4.bin", 2 * 1024 * 1024), dst = dir.filePath("dst4.bin");
  QFile::remove(dst);
  auto r = run(src, dst, true, true, -1, /*cutAt*/ 1000000);
  QVERIFY(r.second.done);
  QCOMPARE(int(r.second.op), int(FO_Error));
  QVERIFY2(!QFile::exists(dst), "partial file must be deleted");
}

static QByteArray frame(const QByteArray& payload) {
  QByteArray out;
  QDataStream s(&out, QIODevice::WriteOnly);
  s.setVersion(QDataStream::Qt_5_15);
  s << quint32(payload.size());
  out.append(payload);
  return out;
}

void TstTransfer::msgStreamSplitHeader() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QTcpSocket client;
  client.connectToHost(QHostAddress::LocalHost, server.serverPort());
  QVERIFY(client.waitForConnected(3000));
  QVERIFY(server.waitForNewConnection(3000));
  QTcpSocket* accepted = server.nextPendingConnection();
  MsgStream stream("me", "peer", "127.0.0.1", 0);
  stream.init(accepted);
  QStringList got;
  connect(&stream, &MsgStream::messageReceived, [&](QString*, QString*, QByteArray& d) { got << QString::fromUtf8(d); });

  QByteArray all = frame("hello") + frame("") + frame(QByteArray(100000, 'x'));
  for(int i = 0; i < all.size(); i += 3) {   // 3-byte pieces split every header
    client.write(all.mid(i, 3));
    client.flush();
    if(i % 600 == 0) QTest::qWait(1);
  }
  QTRY_COMPARE_WITH_TIMEOUT(got.size(), 3, 5000);
  QCOMPARE(got[0], QString("hello"));
  QCOMPARE(got[1], QString(""));
  QCOMPARE(got[2].size(), 100000);
}

void TstTransfer::msgStreamRejectsHugeFrame() {
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QTcpSocket client;
  client.connectToHost(QHostAddress::LocalHost, server.serverPort());
  QVERIFY(client.waitForConnected(3000));
  QVERIFY(server.waitForNewConnection(3000));
  QTcpSocket* accepted = server.nextPendingConnection();
  MsgStream stream("me", "peer", "127.0.0.1", 0);
  stream.init(accepted);
  int msgs = 0, lost = 0;
  connect(&stream, &MsgStream::messageReceived, [&](QString*, QString*, QByteArray&) { ++msgs; });
  connect(&stream, &MsgStream::connectionLost, [&](QString*) { ++lost; });
  QByteArray bad;
  QDataStream s(&bad, QIODevice::WriteOnly);
  s << quint32(0xFFFFFFF0u);
  client.write(bad);
  client.flush();
  QTRY_COMPARE_WITH_TIMEOUT(lost, 1, 5000);
  QCOMPARE(msgs, 0);
}

QTEST_GUILESS_MAIN(TstTransfer)
#include "tst_transfer.moc"
