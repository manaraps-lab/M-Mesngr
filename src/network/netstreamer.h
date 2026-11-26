// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2010 - 2012 Qualia Digital Solutions.
// Copyright (c) 2026 LAN Messenger.
#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <memory>
#include "core/definitions.h"

class QTcpSocket;
class QTimer;
class QFile;
class FileDigest;
class FileCipher;

/****************************************************************************
** Class: FileSender
** Description: Handles sending files.
****************************************************************************/
class FileSender : public QObject {
  Q_OBJECT

public:
  FileSender(void);
  FileSender(QString szId, QString szLocalId, QString szPeerId, QString szFilePath, QString szFileName, qint64 nFileSize,
             QString szAddress, int nPort, FileType nType);
  ~FileSender(void);

  void init(void);
  void stop(void);
  //  Append a SHA-256 of the file after its bytes. Only enabled when the peer
  //  agreed to it (XN_FILEHASH), so older peers keep working unchanged.
  void setDigestEnabled(bool enabled);
  //  Encrypt the content with AES-256-GCM using this per-file key (see
  //  FileCipher). Only used when the peer agreed to it; requires the digest.
  void setEncryptionKey(const QByteArray& key);

  QString id;
  QString peerId;
  FileType type;

signals:
  void progressUpdated(FileMode mode, FileOp fileOp, FileType type, QString* lpszId, QString* lpszUserId, QString* lpszData);

private slots:
  void connected(void);
  void disconnected(void);
  void readyRead(void);
  void bytesWritten(qint64 bytes);
  void timer_timeout(void);

private:
  void sendFile(void);
  void pump(void);
  void fail(void);
  bool writeContent(const char* data, qint64 length);

  QString localId;
  QString filePath;
  QString fileName;
  qint64 fileSize;
  QString address;
  int port;
  qint64 sentBytes;  //  bytes of file content handed to the socket
  QTcpSocket* socket;
  QFile* file;
  QByteArray buffer;
  bool active = false;
  bool finishing = false;
  bool digestEnabled = false;
  std::unique_ptr<FileDigest> digest;
  std::unique_ptr<FileCipher> cipher;
  QTimer* timer;
};

/****************************************************************************
** Class: FileReceiver
** Description: Handles receiving files.
****************************************************************************/
class FileReceiver : public QObject {
  Q_OBJECT

public:
  FileReceiver(void);
  FileReceiver(QString szId, QString szPeerId, QString szFilePath, QString szFileName, qint64 nFileSize,
               QString szAddress, int nPort, FileType nType);
  ~FileReceiver(void);

  void init(QTcpSocket* socket);
  void stop(void);
  //  Expect a SHA-256 trailer after the file bytes and verify it. A corrupted
  //  file is deleted and reported as an error, never as complete.
  void setDigestEnabled(bool enabled);
  //  Expect AES-256-GCM records (see FileCipher) instead of raw content.
  void setEncryptionKey(const QByteArray& key);

  QString id;
  QString peerId;
  FileType type;

signals:
  void progressUpdated(FileMode mode, FileOp fileOp, FileType type, QString* lpszId, QString* lpszUserId, QString* lpszData);

private slots:
  void disconnected(void);
  void readyRead(void);
  void timer_timeout(void);

private:
  void receiveFile(void);
  void fail(void);
  //  Feeds decrypted/raw content bytes to the file and the digest check.
  //  Returns false when the transfer ended (completed or failed).
  bool consume(const char* data, qint64 length);

  QString filePath;
  QString fileName;
  qint64 fileSize;
  QString address;
  int port;
  qint64 sentBytes;  //  bytes of file content received so far
  QTcpSocket* socket;
  QFile* file;
  QByteArray buffer;
  bool active = false;
  bool digestEnabled = false;
  std::unique_ptr<FileDigest> digest;
  QByteArray trailer;  //  digest bytes received after the content
  std::unique_ptr<FileCipher> cipher;
  QByteArray encIn;  //  encrypted bytes received but not yet a whole record
  QTimer* timer;
  int numTimeOuts;
  qint64 lastPosition;
};

/****************************************************************************
** Class: MsgStream
** Description: Handles transmission and reception of TCP streaming messages.
****************************************************************************/
class MsgStream : public QObject {
  Q_OBJECT

public:
  MsgStream(void);
  MsgStream(QString szLocalId, QString szPeerId, QString szPeerAddress, int nPort);
  ~MsgStream(void);

  void init(void);
  void init(QTcpSocket* socket);
  void stop(void);
  void sendMessage(QByteArray& data);

  //  Largest frame accepted from a peer; a larger length field closes the
  //  connection instead of letting a remote host make us buffer without bound.
  static constexpr quint32 kMaxFrameLength = 16 * 1024 * 1024;

signals:
  void connectionLost(QString* lpszUserId);
  void messageReceived(QString* lpszUserId, QString* lpszAddress, QByteArray& data);

private slots:
  void connected(void);
  void disconnected(void);
  void readyRead(void);
  void bytesWritten(qint64 bytes);

private:
  QTcpSocket* socket;
  int port;
  QString localId;
  QString peerId;
  QString peerAddress;
  QByteArray outData;
  QByteArray inData;  //  bytes received but not yet forming a whole frame
  quint32 outDataLen;
  quint32 inDataLen;  //  length of the frame being read (valid when reading)
  bool reading;       //  frame header already parsed
};
