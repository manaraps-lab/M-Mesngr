// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2010 - 2012 Qualia Digital Solutions.
// Copyright (c) 2026 LAN Messenger.
#include <QDir>
#include <QFileInfo>
#include <QDataStream>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QPointer>
#include "core/filedigest.h"
#include "core/filecipher.h"
#include "core/lmccategories.h"
#include "netstreamer.h"

//  File content is moved in 256 KiB chunks. The sender keeps the socket's
//  write queue between kLowWater and kHighWater so the disk and the network
//  overlap instead of alternating one small chunk at a time.
const qint64 bufferSize = 256 * 1024;
const qint64 kHighWater = 1024 * 1024;
const int kSocketBuffer = 1024 * 1024;

/****************************************************************************
** Class: FileSender
** Description: Handles sending files.
****************************************************************************/
FileSender::FileSender(void) {
}

FileSender::FileSender(QString szId, QString szLocalId, QString szPeerId, QString szFilePath,
                       QString szFileName, qint64 nFileSize, QString szAddress, int nPort, FileType nType) {
  id = szId;
  localId = szLocalId;
  peerId = szPeerId;
  filePath = szFilePath;
  fileName = szFileName;
  fileSize = nFileSize;
  address = szAddress;
  port = nPort;
  sentBytes = 0;
  active = false;
  finishing = false;
  file = NULL;
  socket = NULL;
  timer = NULL;
  type = nType;
}

FileSender::~FileSender(void) {
}

void FileSender::setDigestEnabled(bool enabled) {
  digestEnabled = enabled;
}

void FileSender::setEncryptionKey(const QByteArray& key) {
  cipher.reset(new FileCipher(key));
  if(!cipher->isValid())
    cipher.reset();
}

//  Hands content (or the digest) to the socket, as one encrypted record when a
//  key was agreed.
bool FileSender::writeContent(const char* data, qint64 length) {
  if(!cipher)
    return socket->write(data, length) == length;
  QByteArray record = cipher->seal(data, length);
  if(record.isEmpty())
    return false;
  return socket->write(record) == record.size();
}

void FileSender::init(void) {
  socket = new QTcpSocket(this);
  socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, kSocketBuffer);
  connect(socket, &QAbstractSocket::connected, this, &FileSender::connected);
  connect(socket, &QAbstractSocket::disconnected, this, &FileSender::disconnected);
  connect(socket, &QIODevice::readyRead, this, &FileSender::readyRead);
  connect(socket, &QIODevice::bytesWritten, this, &FileSender::bytesWritten);

  QHostAddress hostAddress(address);
  socket->connectToHost(hostAddress, port);
}

void FileSender::stop(void) {
  active = false;
  finishing = false;

  if(timer)
    timer->stop();
  if(file && file->isOpen())
    file->close();
  if(socket && socket->isOpen())
    socket->close();
}

void FileSender::connected(void) {
  QByteArray data = id.toLocal8Bit();
  data.append(localId.toLocal8Bit());
  data.insert(0, "FILE"); // insert indicator that this socket handles file transfer
  //	send an id message and then wait for a START message
  //	from receiver, which will trigger readyRead signal
  socket->write(data);
}

void FileSender::disconnected(void) {
  if(finishing) {
    finishing = false;
    //  Graceful close hands every byte to the OS in order, but it cannot
    //  prove delivery to the peer app (that is the receiver's FO_Complete).
    //  A break during the drain must not report success.
    if(socket->error() == QAbstractSocket::UnknownSocketError) {
      emit progressUpdated(FM_Send, FO_Complete, type, &id, &peerId, &filePath);
    } else {
      QString data;
      emit progressUpdated(FM_Send, FO_Error, type, &id, &peerId, &data);
    }
    return;
  }

  if(active) {
    active = false;
    QString data;
    emit progressUpdated(FM_Send, FO_Error, type, &id, &peerId, &data);
  }
}

void FileSender::readyRead(void) {
  //	message received from receiver (START), begin sending the file. Ignore
  //	anything the peer sends afterwards.
  if(active || finishing || file)
    return;
  socket->readAll();
  sendFile();
}

void FileSender::timer_timeout(void) {
  if(!active)
    return;

  //  Report what has left the machine, not what is merely queued in the socket.
  qint64 transferred = sentBytes - socket->bytesToWrite();
  if(transferred < 0)
    transferred = 0;
  QString text = QString::number(transferred);
  emit progressUpdated(FM_Send, FO_Progress, type, &id, &peerId, &text);
}

void FileSender::bytesWritten(qint64 bytes) {
  Q_UNUSED(bytes);
  pump();
}

void FileSender::fail(void) {
  active = false;
  if(timer)
    timer->stop();
  if(file && file->isOpen())
    file->close();
  socket->abort();
  QString data;
  emit progressUpdated(FM_Send, FO_Error, type, &id, &peerId, &data);
}

//  Moves file data into the socket until the write queue is full or the file
//  has been read completely.
void FileSender::pump(void) {
  while(active && socket->bytesToWrite() < kHighWater) {
    qint64 remaining = fileSize - sentBytes;
    if(remaining <= 0) {
      //  Whole file is read from disk, but bytes may still sit in socket
      //  buffers. Close gracefully and report completion on disconnect.
      active = false;
      finishing = true;
      file->close();
      if(timer)
        timer->stop();
      if(digestEnabled && digest) {
        QByteArray sum = digest->finish();
        if(sum.size() != 32) {
          finishing = false;
          fail();
          return;
        }
        if(!writeContent(sum.constData(), sum.size())) {
          finishing = false;
          fail();
          return;
        }
      }
      socket->disconnectFromHost();
      return;
    }

    qint64 want = remaining < bufferSize ? remaining : bufferSize;
    qint64 got = file->read(buffer.data(), want);
    if(got <= 0) {
      //  File shrank or became unreadable after the size was announced.
      fail();
      return;
    }
    if(digestEnabled && digest)
      digest->update(buffer.constData(), got);
    if(!writeContent(buffer.constData(), got)) {
      fail();
      return;
    }
    sentBytes += got;
  }
}

void FileSender::sendFile(void) {
  file = new QFile(filePath, this);

  if(file->open(QIODevice::ReadOnly)) {
    buffer.resize(bufferSize);
    sentBytes = 0;
    active = true;
    if(digestEnabled)
      digest.reset(new FileDigest());

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &FileSender::timer_timeout);
    timer->start(PROGRESS_TIMEOUT);

    pump();
  } else {
    socket->close();
    QString data;
    emit progressUpdated(FM_Send, FO_Error, type, &id, &peerId, &data);
  }
}


/****************************************************************************
** Class: FileReceiver
** Description: Handles receiving files.
****************************************************************************/
FileReceiver::FileReceiver(void) {
}

FileReceiver::FileReceiver(QString szId, QString szPeerId, QString szFilePath, QString szFileName,
                           qint64 nFileSize, QString szAddress, int nPort, FileType nType) {
  id = szId;
  peerId = szPeerId;
  filePath = szFilePath;
  fileName = szFileName;
  fileSize = nFileSize;
  address = szAddress;
  port = nPort;
  sentBytes = 0;
  active = false;
  file = NULL;
  socket = NULL;
  timer = NULL;
  type = nType;
  lastPosition = 0;
  numTimeOuts = 0;
}

FileReceiver::~FileReceiver(void) {
}

void FileReceiver::setDigestEnabled(bool enabled) {
  digestEnabled = enabled;
}

void FileReceiver::setEncryptionKey(const QByteArray& key) {
  cipher.reset(new FileCipher(key));
  if(!cipher->isValid())
    cipher.reset();
}

void FileReceiver::init(QTcpSocket* socket) {
  this->socket = socket;
  socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, kSocketBuffer);
  connect(socket, &QAbstractSocket::disconnected, this, &FileReceiver::disconnected);
  connect(this->socket, &QIODevice::readyRead, this, &FileReceiver::readyRead);

  receiveFile();
  //	now send a START message to sender (only if the file could be opened)
  if(!active)
    return;
  socket->write("START");

  //  An empty file has no content bytes, so nothing will trigger readyRead.
  //  With a digest the 32 byte trailer still arrives and completes it there.
  if(fileSize <= 0 && !digestEnabled) {
    active = false;
    if(timer)
      timer->stop();
    file->close();
    socket->close();
    emit progressUpdated(FM_Receive, FO_Complete, type, &id, &peerId, &filePath);
  }
}

void FileReceiver::stop(void) {
  //  Anything still active is incomplete (completion clears `active`), and an
  //  unverified file must not be left behind either.
  bool deleteFile = active;

  active = false;

  if(timer)
    timer->stop();
  if(file && file->isOpen())
    file->close();
  if(socket && socket->isOpen())
    socket->close();

  if(deleteFile)
    QFile::remove(filePath);
}

void FileReceiver::fail(void) {
  active = false;
  if(timer)
    timer->stop();
  if(file && file->isOpen())
    file->close();
  QFile::remove(filePath); // no partial or corrupt file left behind
  socket->close();
  QString data;
  emit progressUpdated(FM_Receive, FO_Error, type, &id, &peerId, &data);
}

void FileReceiver::disconnected(void) {
  if(active) {
    active = false;
    if(timer)
      timer->stop();
    if(file && file->isOpen())
      file->close();
    QFile::remove(filePath);
    QString data;
    emit progressUpdated(FM_Receive, FO_Error, type, &id, &peerId, &data);
  }
}

void FileReceiver::readyRead(void) {
  if(!active)
    return;

  if(cipher) {
    //  Encrypted stream: collect bytes, then open every complete record.
    encIn.append(socket->readAll());
    while(active) {
      if(encIn.size() < FileCipher::kHeaderSize)
        return;
      const unsigned char* h = reinterpret_cast<const unsigned char*>(encIn.constData());
      const quint32 n = (quint32(h[0]) << 24) | (quint32(h[1]) << 16) | (quint32(h[2]) << 8) | quint32(h[3]);
      if(n < 1 || n > quint32(FileCipher::kMaxRecord)) {
        qCWarning(lmcNetTcp(), "File %s from %s: invalid record length, discarding", qPrintable(id), qPrintable(peerId));
        fail();
        return;
      }
      const qint64 total = FileCipher::kHeaderSize + qint64(n) + FileCipher::kTagSize;
      if(encIn.size() < total)
        return;
      QByteArray plain;
      if(!cipher->open(encIn.constData() + FileCipher::kHeaderSize, n, encIn.constData() + FileCipher::kHeaderSize + n,
                       plain)) {
        qCWarning(lmcNetTcp(), "File %s from %s failed decryption, discarding", qPrintable(id), qPrintable(peerId));
        fail();
        return;
      }
      encIn.remove(0, static_cast<int>(total));
      if(!consume(plain.constData(), plain.size()))
        return;
    }
    return;
  }

  //	Drain everything available: several chunks may arrive between event
  //	loop turns, but Qt emits readyRead only once for them. Reading a
  //	single chunk per signal leaves the tail buffered with no new signal
  //	to wake us, stalling the transfer short of fileSize.
  while(active && socket->bytesAvailable() > 0) {
    qint64 contentLeft = fileSize - sentBytes;
    qint64 toRead = contentLeft > 0 ? (contentLeft < bufferSize ? contentLeft : bufferSize)
                                    : (32 - trailer.size());
    if(toRead <= 0)
      toRead = bufferSize;
    qint64 bytesReceived = socket->read(buffer.data(), toRead);
    if(bytesReceived <= 0)
      break;
    if(!consume(buffer.constData(), bytesReceived))
      return;
  }
}

bool FileReceiver::consume(const char* data, qint64 length) {
  qint64 contentLeft = fileSize - sentBytes;
  qint64 intoFile = contentLeft > 0 ? (length < contentLeft ? length : contentLeft) : 0;
  if(intoFile > 0) {
    if(file->write(data, intoFile) != intoFile) {
      fail();
      return false;
    }
    if(digestEnabled && digest)
      digest->update(data, intoFile);
    sentBytes += intoFile;
  }
  if(length > intoFile)
    trailer.append(data + intoFile, static_cast<int>(length - intoFile));

  if(sentBytes < fileSize)
    return true;

  if(file->isOpen())
    file->close();
  if(!digestEnabled) {
    active = false;
    if(timer)
      timer->stop();
    socket->close();
    emit progressUpdated(FM_Receive, FO_Complete, type, &id, &peerId, &filePath);
    return false;
  }
  if(trailer.size() >= 32) {
    //  All content arrived; it counts as complete only if it matches
    //  what the sender hashed.
    QByteArray expected = digest ? digest->finish() : QByteArray();
    if(expected.size() != 32 || trailer.left(32) != expected) {
      qCWarning(lmcNetTcp(), "File %s from %s failed SHA-256 verification, discarding",
                qPrintable(id), qPrintable(peerId));
      fail();
      return false;
    }
    active = false;
    if(timer)
      timer->stop();
    socket->close();
    emit progressUpdated(FM_Receive, FO_Complete, type, &id, &peerId, &filePath);
    return false;
  }
  return true;
}

void FileReceiver::timer_timeout(void) {
  if(!active)
    return;

  if(lastPosition < sentBytes) {
    lastPosition = sentBytes;
    numTimeOuts = 0;
  } else {
    numTimeOuts++;
    if(numTimeOuts > 20) {
      QString data;
      emit progressUpdated(FM_Receive, FO_Error, type, &id, &peerId, &data);
      stop();
      return;
    }
  }

  QString transferred = QString::number(sentBytes);
  emit progressUpdated(FM_Receive, FO_Progress, type, &id, &peerId, &transferred);
}

void FileReceiver::receiveFile(void) {
  QDir dir = QFileInfo(filePath).dir();
  if(!dir.exists())
    dir.mkpath(dir.absolutePath());

  file = new QFile(filePath, this);

  if(file->open(QIODevice::WriteOnly)) {
    buffer.resize(bufferSize);
    sentBytes = 0;
    active = true;
    if(digestEnabled)
      digest.reset(new FileDigest());

    //  Zero-length files carry no content: handled by readyRead via the
    //  trailer (or, without a digest, completed right here).
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &FileReceiver::timer_timeout);
    timer->start(PROGRESS_TIMEOUT);
  } else {
    socket->close();
    emit progressUpdated(FM_Receive, FO_Error, type, &id, &peerId, &filePath);
  }
}


/****************************************************************************
** Class: MsgStream
** Description: Handles transmission and reception of TCP streaming messages.
****************************************************************************/
MsgStream::MsgStream(void) {
  socket = NULL;
  reading = false;
}

MsgStream::MsgStream(QString szLocalId, QString szPeerId, QString szPeerAddress, int nPort) {
  localId = szLocalId;
  peerId = szPeerId;
  peerAddress = szPeerAddress;
  port = nPort;
  socket = NULL;
  reading = false;
  outDataLen = 0;
  inDataLen = 0;
}

MsgStream::~MsgStream(void) {
}

void MsgStream::init(void) {
  socket = new QTcpSocket(this);
  connect(socket, &QAbstractSocket::connected, this, &MsgStream::connected);
  connect(socket, &QAbstractSocket::disconnected, this, &MsgStream::disconnected);
  connect(socket, &QIODevice::readyRead, this, &MsgStream::readyRead);
  connect(socket, &QIODevice::bytesWritten, this, &MsgStream::bytesWritten);

  QHostAddress hostAddress(peerAddress);
  socket->connectToHost(hostAddress, port);
}

void MsgStream::init(QTcpSocket* socket) {
  this->socket = socket;
  connect(socket, &QAbstractSocket::disconnected, this, &MsgStream::disconnected);
  connect(this->socket, &QIODevice::readyRead, this, &MsgStream::readyRead);
  connect(socket, &QIODevice::bytesWritten, this, &MsgStream::bytesWritten);
}

void MsgStream::stop(void) {
  if(socket && socket->isOpen())
    socket->close();
}

void MsgStream::sendMessage(QByteArray& data) {
  qint32 dataLen = sizeof(quint32) + data.length();
  outDataLen += dataLen;
  outData.resize(dataLen);

  QDataStream stream(&outData, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_5_15);
  stream << (quint32)data.length();
  stream.writeRawData(data.data(), data.length());

  qint64 numBytesWritten = socket->write(outData);
  if(numBytesWritten < 0)
    qCCritical(lmcNet(), "Socket write failed");
}

void MsgStream::connected(void) {
  outData = localId.toLocal8Bit();
  outData.insert(0, "MSG"); // insert indicator that this socket handles messages
  outDataLen = outData.length();

  //	send an id message and then wait for public key message
  //	from receiver, which will trigger readyRead signal
  qint64 numBytesWritten = socket->write(outData);
  if(numBytesWritten < 0)
    qCCritical(lmcNet(), "Socket write failed");
}

void MsgStream::disconnected(void) {
  emit connectionLost(&peerId);
}

void MsgStream::readyRead(void) {
  //  Frames are a 4-byte big-endian length followed by that many bytes. A
  //  frame may arrive in any number of pieces (including a split header), so
  //  everything is accumulated and cut into frames here.
  inData.append(socket->readAll());

  QPointer<MsgStream> guard(this);  // a handler may delete this stream
  for(;;) {
    if(!reading) {
      if(inData.size() < 4)
        return;
      QDataStream stream(inData.left(4));
      stream.setVersion(QDataStream::Qt_5_15);
      stream >> inDataLen;
      if(inDataLen > kMaxFrameLength) {
        qCWarning(lmcNet(), "Dropping connection: frame of %u bytes exceeds limit", inDataLen);
        inData.clear();
        socket->abort();
        return;
      }
      inData.remove(0, 4);
      reading = true;
    }

    if(static_cast<quint32>(inData.size()) < inDataLen)
      return;

    QByteArray frame = inData.left(inDataLen);
    inData.remove(0, inDataLen);
    reading = false;
    emit messageReceived(&peerId, &peerAddress, frame);
    if(!guard)
      return;
  }
}

void MsgStream::bytesWritten(qint64 bytes) {
  outDataLen -= bytes;
  if(outDataLen == 0)
    return;

  if(outDataLen > 0)
    qCWarning(lmcNet(), "Socket write operation not completed");
  if(outDataLen < 0)
    qCWarning(lmcNet(), "Socket write overrun");

  //	TODO: handle situation when entire message is not written to stream in one write operation
  //	The following code is not functional currently, hence commented out.
  /*outData = outData.mid(outDataLen);
	socket->write(outData);*/
}
