// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 MANARA Messenger contributors.
#include <QtTest>

#include "core/pathsafety.h"

class TstPathSafety : public QObject {
  Q_OBJECT
private slots:
  void fileNameTraversal_data();
  void fileNameTraversal();
  void reservedAndTrailing();
  void longName();
  void relativePathRejects_data();
  void relativePathRejects();
  void relativePathAccepts();
  void insideCheck();
};

void TstPathSafety::fileNameTraversal_data() {
  QTest::addColumn<QString>("in");
  QTest::addColumn<QString>("out");
  QTest::newRow("plain") << "report.pdf" << "report.pdf";
  QTest::newRow("unix traversal") << "../../etc/passwd" << "passwd";
  QTest::newRow("win traversal") << "..\\..\\Startup\\x.exe" << "x.exe";
  QTest::newRow("absolute win") << "C:\\Windows\\evil.dll" << "evil.dll";
  QTest::newRow("drive relative") << "C:evil.dll" << "C_evil.dll";
  QTest::newRow("only dots") << ".." << "file";
  QTest::newRow("empty") << "" << "file";
  QTest::newRow("illegal chars") << "a<b>c|d?.txt" << "a_b_c_d_.txt";
  QTest::newRow("control chars") << QString("a\x01\x1f" "b.txt") << "a__b.txt";
  QTest::newRow("arabic kept") << QString::fromUtf8("تقرير.docx") << QString::fromUtf8("تقرير.docx");
  QTest::newRow("ads stream") << "file.txt:hidden" << "file.txt_hidden";
}

void TstPathSafety::fileNameTraversal() {
  QFETCH(QString, in);
  QFETCH(QString, out);
  QCOMPARE(PathSafety::sanitizeFileName(in), out);
}

void TstPathSafety::reservedAndTrailing() {
  QCOMPARE(PathSafety::sanitizeFileName("CON"), QString("_CON"));
  QCOMPARE(PathSafety::sanitizeFileName("nul.txt"), QString("_nul.txt"));
  QCOMPARE(PathSafety::sanitizeFileName("COM1"), QString("_COM1"));
  QCOMPARE(PathSafety::sanitizeFileName("lpt9.log"), QString("_lpt9.log"));
  QCOMPARE(PathSafety::sanitizeFileName("COM10"), QString("COM10"));
  QCOMPARE(PathSafety::sanitizeFileName("name. "), QString("name"));
  QCOMPARE(PathSafety::sanitizeFileName("  lead.txt"), QString("lead.txt"));
}

void TstPathSafety::longName() {
  const QString name = QString(500, 'a') + ".pdf";
  const QString out = PathSafety::sanitizeFileName(name);
  QVERIFY(out.size() <= PathSafety::kMaxComponentLength);
  QVERIFY(out.endsWith(".pdf"));
}

void TstPathSafety::relativePathRejects_data() {
  QTest::addColumn<QString>("in");
  QTest::newRow("parent") << "../x";
  QTest::newRow("nested parent") << "a/../../x";
  QTest::newRow("backslash parent") << "a\\..\\..\\x";
  QTest::newRow("absolute unix") << "/etc/passwd";
  QTest::newRow("absolute win") << "\\Windows\\x";
  QTest::newRow("drive") << "C:/x";
  QTest::newRow("drive relative") << "D:x";
  QTest::newRow("empty") << "";
  QTest::newRow("only dot") << ".";
  QTest::newRow("too deep") << QString("a/").repeated(PathSafety::kMaxDepth + 1) + "f";
  QTest::newRow("too long") << QString(PathSafety::kMaxRelativeLength + 1, 'a');
}

void TstPathSafety::relativePathRejects() {
  QFETCH(QString, in);
  QVERIFY2(PathSafety::sanitizeRelativePath(in).isEmpty(), qPrintable(in));
}

void TstPathSafety::relativePathAccepts() {
  QCOMPARE(PathSafety::sanitizeRelativePath("photos/2026/a.jpg"), QString("photos/2026/a.jpg"));
  QCOMPARE(PathSafety::sanitizeRelativePath("photos\\2026\\a.jpg"), QString("photos/2026/a.jpg"));
  QCOMPARE(PathSafety::sanitizeRelativePath("./a//b.txt"), QString("a/b.txt"));
  QCOMPARE(PathSafety::sanitizeRelativePath("dir/CON/x"), QString("dir/_CON/x"));
}

void TstPathSafety::insideCheck() {
  QVERIFY(PathSafety::isInside("/data/in", "/data/in/a/b.txt"));
  QVERIFY(PathSafety::isInside("/data/in", "/data/in"));
  QVERIFY(!PathSafety::isInside("/data/in", "/data/in/../out/x"));
  QVERIFY(!PathSafety::isInside("/data/in", "/data/inx/a"));
  QVERIFY(!PathSafety::isInside("/data/in", "/etc/passwd"));
  QVERIFY(!PathSafety::isInside("", "/a"));
}

QTEST_APPLESS_MAIN(TstPathSafety)
#include "tst_pathsafety.moc"
