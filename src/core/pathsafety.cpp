// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
#include "pathsafety.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace PathSafety {

namespace {

bool isReservedDeviceName(const QString& component) {
  // The device name is whatever precedes the first dot ("con.txt" is CON).
  const int dot = component.indexOf(QLatin1Char('.'));
  const QString stem = (dot < 0 ? component : component.left(dot)).trimmed().toUpper();
  static const QStringList fixed = {QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"),
                                    QStringLiteral("NUL"), QStringLiteral("CONIN$"), QStringLiteral("CONOUT$")};
  if(fixed.contains(stem))
    return true;
  if(stem.size() == 4 && (stem.startsWith(QLatin1String("COM")) || stem.startsWith(QLatin1String("LPT")))) {
    const QChar digit = stem.at(3);
    return digit >= QLatin1Char('0') && digit <= QLatin1Char('9');
  }
  return false;
}

}  // namespace

QString sanitizeComponent(const QString& component) {
  QString out;
  out.reserve(component.size());
  for(const QChar ch : component) {
    const ushort u = ch.unicode();
    if(u < 0x20 || u == 0x7F || ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char(':') ||
       ch == QLatin1Char('"') || ch == QLatin1Char('|') || ch == QLatin1Char('?') || ch == QLatin1Char('*') ||
       ch == QLatin1Char('\\') || ch == QLatin1Char('/')) {
      out.append(QLatin1Char('_'));
    } else {
      out.append(ch);
    }
  }

  // Windows silently drops trailing dots and spaces, which would let
  // "evil. " alias an existing name; remove them (and leading spaces) here.
  out = out.trimmed();
  while(out.endsWith(QLatin1Char('.')) || out.endsWith(QLatin1Char(' ')))
    out.chop(1);
  if(out.isEmpty())
    return QString();

  if(isReservedDeviceName(out))
    out.prepend(QLatin1Char('_'));

  if(out.size() > kMaxComponentLength) {
    const int dot = out.lastIndexOf(QLatin1Char('.'));
    const QString ext = (dot > 0 && out.size() - dot <= 20) ? out.mid(dot) : QString();
    out = out.left(kMaxComponentLength - ext.size()) + ext;
    while(out.endsWith(QLatin1Char('.')) || out.endsWith(QLatin1Char(' ')))
      out.chop(1);
  }
  return out;
}

QString sanitizeFileName(const QString& name, const QString& fallback) {
  QString normalised = name;
  normalised.replace(QLatin1Char('\\'), QLatin1Char('/'));
  const QStringList parts = normalised.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  // Take the last usable component; ".." and similar are dropped by
  // sanitizeComponent returning an empty string.
  for(int i = parts.size() - 1; i >= 0; --i) {
    const QString clean = sanitizeComponent(parts.at(i));
    if(!clean.isEmpty())
      return clean;
    if(parts.at(i) != QLatin1String(".") && parts.at(i) != QLatin1String(".."))
      break;
  }
  return fallback;
}

QString sanitizeRelativePath(const QString& relativePath) {
  if(relativePath.isEmpty() || relativePath.size() > kMaxRelativeLength)
    return QString();

  QString normalised = relativePath;
  normalised.replace(QLatin1Char('\\'), QLatin1Char('/'));

  // Absolute paths and drive letters ("C:foo", "C:/foo") are never valid.
  if(normalised.startsWith(QLatin1Char('/')))
    return QString();
  static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:"));
  if(drive.match(normalised).hasMatch())
    return QString();

  QStringList cleaned;
  const QStringList parts = normalised.split(QLatin1Char('/'), Qt::KeepEmptyParts);
  for(const QString& part : parts) {
    if(part.isEmpty() || part == QLatin1String("."))
      continue;
    if(part == QLatin1String(".."))
      return QString();
    const QString clean = sanitizeComponent(part);
    if(clean.isEmpty())
      return QString();
    cleaned.append(clean);
  }
  if(cleaned.isEmpty() || cleaned.size() > kMaxDepth)
    return QString();
  return cleaned.join(QLatin1Char('/'));
}

bool isInside(const QString& baseDir, const QString& path) {
  if(baseDir.isEmpty() || path.isEmpty())
    return false;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
  const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
  const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
  QString base = QDir::cleanPath(QFileInfo(baseDir).absoluteFilePath());
  const QString target = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  if(base.compare(target, cs) == 0)
    return true;
  if(!base.endsWith(QLatin1Char('/')))
    base.append(QLatin1Char('/'));
  return target.startsWith(base, cs);
}

}  // namespace PathSafety
