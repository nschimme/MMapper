#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <QObject>

class TestRemoteEditWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFindNext();
    void testFindPrevious();
    void testReplace();
    void testReplaceAll();
    void testGotoLine();
    void testFindWrapAround();

private:
    // Helper methods if needed
};
