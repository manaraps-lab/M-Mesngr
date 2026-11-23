// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
//
// Helpers that make file names and relative paths received from a remote
// peer safe to use below a local directory. A peer is untrusted input: a
// name such as "..\..\Startup\x.exe" or "C:\x" must never leave the
// receive folder.
#pragma once

#include <QString>

namespace PathSafety {

// Longest single path component we accept, in UTF-16 code units.
constexpr int kMaxComponentLength = 180;
// Deepest relative path we accept (folder transfers).
constexpr int kMaxDepth = 32;
// Longest relative path we accept, in UTF-16 code units.
constexpr int kMaxRelativeLength = 1024;

// Makes one path component safe: control characters and  < > : " | ? * \ /
// become '_', leading/trailing spaces and trailing dots are removed, Windows
// reserved device names (CON, NUL, COM1 ...) get a '_' prefix and over-long
// names are shortened (the extension is kept). Returns an empty string when
// nothing usable is left (for example ".." or "...").
QString sanitizeComponent(const QString& component);

// Reduces a peer supplied file name to one safe component. Any directory
// part is dropped. Returns `fallback` when nothing usable is left.
QString sanitizeFileName(const QString& name, const QString& fallback = QStringLiteral("file"));

// Validates a peer supplied relative path (folder transfers) and returns it
// normalised with '/' separators. Returns an empty string when the path is
// rejected: absolute paths, drive letters, "..", too deep or too long.
QString sanitizeRelativePath(const QString& relativePath);

// True when `path` lies inside `baseDir` (or is `baseDir` itself). The check
// is lexical (it cleans "." and ".." but does not resolve symbolic links) and
// case-insensitive on Windows and macOS.
bool isInside(const QString& baseDir, const QString& path);

}  // namespace PathSafety
