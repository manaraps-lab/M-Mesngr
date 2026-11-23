// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
//
// Loopback throughput benchmark for FileSender/FileReceiver.
//   transferbench <sizeMB> <runs> [digest]
// Loopback measures the software path (disk, copies, hashing, event loop), NOT
// a real network: it is an upper bound and must not be quoted as LAN speed.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "tools/loopback.h"

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  const int sizeMB = argc > 1 ? QString(argv[1]).toInt() : 512;
  const int runs = argc > 2 ? QString(argv[2]).toInt() : 3;
  const bool digest = argc > 3 && QString(argv[3]) == "digest";
  QTextStream out(stdout);

  QTemporaryDir dir;
  const QString src = dir.filePath("src.bin"), dst = dir.filePath("dst.bin");
  {
    QFile f(src);
    f.open(QIODevice::WriteOnly);
    QByteArray block(1 << 20, 0);
    quint32 x = 1;
    for(int m = 0; m < sizeMB; ++m) {
      for(int i = 0; i < block.size(); i += 4) { x = x * 1664525u + 1013904223u; memcpy(block.data() + i, &x, 4); }
      f.write(block);
    }
  }
  const qint64 size = QFileInfo(src).size();

  for(int r = 0; r < runs; ++r) {
    QFile::remove(dst);
    LoopbackServer server;
    FileReceiver receiver(QString(32, 'a'), "alice", dst, "x", size, "127.0.0.1", server.port(), FT_Normal);
    server.setReceiver(&receiver);
    FileSender sender(QString(32, 'a'), "bob", "alice", src, "x", size, "127.0.0.1", server.port(), FT_Normal);
#ifndef BENCH_LEGACY
    receiver.setDigestEnabled(digest);
    sender.setDigestEnabled(digest);
#endif
    bool rxDone = false, txDone = false, ok = true;
    QObject::connect(&receiver, &FileReceiver::progressUpdated, [&](FileMode, FileOp op, FileType, QString*, QString*, QString*) {
      if(op == FO_Complete) rxDone = true;
      if(op == FO_Error) { rxDone = true; ok = false; }
    });
    QObject::connect(&sender, &FileSender::progressUpdated, [&](FileMode, FileOp op, FileType, QString*, QString*, QString*) {
      if(op == FO_Complete) txDone = true;
      if(op == FO_Error) { txDone = true; ok = false; }
    });
    QElapsedTimer t;
    t.start();
    sender.init();
    while(!(rxDone && txDone) && t.elapsed() < 600000)
      QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
    const double sec = t.elapsed() / 1000.0;
    out << "run " << r + 1 << ": " << (ok ? "ok" : "FAILED") << "  " << size / 1048576 << " MiB in " << sec
        << " s = " << (size / 1048576.0) / sec << " MiB/s" << Qt::endl;
  }
  return 0;
}
