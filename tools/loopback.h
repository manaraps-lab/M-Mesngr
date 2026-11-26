// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
//
// Test/benchmark scaffolding: a minimal stand-in for lmcTcpNetwork that accepts
// the "FILE"+id+userId handshake and hands the socket to a FileReceiver, plus a
// proxy that can corrupt or cut the byte stream.
#pragma once

#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include "network/netstreamer.h"

class LoopbackServer : public QObject {
  Q_OBJECT
public:
  explicit LoopbackServer(QObject* parent = nullptr) : QObject(parent) {
    connect(&server, &QTcpServer::newConnection, this, &LoopbackServer::onConnection);
    server.listen(QHostAddress::LocalHost, 0);
  }
  quint16 port() const { return server.serverPort(); }
  void setReceiver(FileReceiver* r) { receiver = r; }

private slots:
  void onConnection() {
    QTcpSocket* s = server.nextPendingConnection();
    connect(s, &QIODevice::readyRead, this, [this, s]() {
      if(s->bytesAvailable() < 37)  // "FILE" + 32 id + at least 1 user id char
        return;
      disconnect(s, &QIODevice::readyRead, nullptr, nullptr);
      s->read(4 + 32 + userIdLen);
      if(receiver)
        receiver->init(s);
    });
  }

public:
  int userIdLen = 5;

private:
  QTcpServer server;
  QPointer<FileReceiver> receiver;
};

// Forwards connections to `targetPort`. `flipAt` >= 0 flips one bit of the
// server->client... here: client->server (sender->receiver) byte at that stream
// offset; `cutAt` >= 0 drops the connection after that many bytes.
//  Pass-through proxy that records everything the sender wrote (after the
//  handshake) so a test can inspect what actually crossed the socket.
class TapProxy : public QObject {
  Q_OBJECT
public:
  TapProxy(quint16 targetPort, QByteArray* sink, QObject* parent = nullptr)
      : QObject(parent), target(targetPort), out(sink) {
    connect(&server, &QTcpServer::newConnection, this, &TapProxy::onConnection);
    server.listen(QHostAddress::LocalHost, 0);
  }
  quint16 port() const { return server.serverPort(); }

private slots:
  void onConnection() {
    QTcpSocket* in = server.nextPendingConnection();
    QTcpSocket* up = new QTcpSocket(this);
    up->connectToHost(QHostAddress::LocalHost, target);
    connect(in, &QIODevice::readyRead, this, [this, in, up]() {
      QByteArray data = in->readAll();
      out->append(data);
      up->write(data);
    });
    connect(up, &QIODevice::readyRead, this, [in, up]() { in->write(up->readAll()); });
    connect(in, &QAbstractSocket::disconnected, up, &QTcpSocket::disconnectFromHost);
    connect(up, &QAbstractSocket::disconnected, in, &QTcpSocket::disconnectFromHost);
  }

private:
  QTcpServer server;
  quint16 target;
  QByteArray* out;
};

class TamperProxy : public QObject {
  Q_OBJECT
public:
  TamperProxy(quint16 targetPort, qint64 flipAt, qint64 cutAt, QObject* parent = nullptr)
      : QObject(parent), target(targetPort), flip(flipAt), cut(cutAt) {
    connect(&server, &QTcpServer::newConnection, this, &TamperProxy::onConnection);
    server.listen(QHostAddress::LocalHost, 0);
  }
  quint16 port() const { return server.serverPort(); }

private slots:
  void onConnection() {
    QTcpSocket* in = server.nextPendingConnection();
    QTcpSocket* out = new QTcpSocket(this);
    out->connectToHost(QHostAddress::LocalHost, target);
    connect(in, &QIODevice::readyRead, this, [this, in, out]() {
      QByteArray data = in->readAll();
      // offsets count from the first byte after the 37-byte handshake
      for(int i = 0; i < data.size(); ++i, ++seen) {
        if(flip >= 0 && seen - 37 == flip)
          data[i] = char(data[i] ^ 0x01);
      }
      if(cut >= 0 && seen > cut) {
        data.truncate(data.size() - int(seen - cut));
        out->write(data);
        out->flush();
        out->disconnectFromHost();
        in->abort();
        return;
      }
      out->write(data);
    });
    connect(out, &QIODevice::readyRead, this, [in, out]() { in->write(out->readAll()); });
    connect(in, &QAbstractSocket::disconnected, out, &QTcpSocket::disconnectFromHost);
    connect(out, &QAbstractSocket::disconnected, in, &QTcpSocket::disconnectFromHost);
  }

private:
  QTcpServer server;
  quint16 target;
  qint64 flip, cut;
  qint64 seen = 0;
};
