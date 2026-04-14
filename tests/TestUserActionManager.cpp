// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestUserActionManager.h"

#include "../src/client/UserActionManager.h"
#include "../src/configuration/configuration.h"

#include <QtTest/QtTest>

TestUserActionManager::TestUserActionManager()
{
    setEnteredMain();
}

TestUserActionManager::~TestUserActionManager() = default;

void TestUserActionManager::testBasicAction()
{
    UserActionManager manager;
    std::ignore = manager.setAction(UserActionType::StartsWith, "Hello", "say Hi");

    std::string executed;
    std::string line = "Hello world";
    manager.evaluate(StringView{line}, [&](const std::string &cmd) { executed = cmd; });

    QCOMPARE(executed, std::string("say Hi"));
}

void TestUserActionManager::testRegexCaptures()
{
    UserActionManager manager;
    std::ignore = manager.setAction(UserActionType::Regex,
                                    "You gain (\\d+) experience",
                                    "emote cheers %1 XP!");

    std::string executed;
    std::string line = "You gain 1234 experience";
    manager.evaluate(StringView{line}, [&](const std::string &cmd) { executed = cmd; });

    QCOMPARE(executed, std::string("emote cheers 1234 XP!"));
}

void TestUserActionManager::testStartsWithEndsWith()
{
    UserActionManager manager;
    std::ignore = manager.setAction(UserActionType::StartsWith, "Starts", "cmd %0");
    std::ignore = manager.setAction(UserActionType::EndsWith, "Ends", "finish %0");

    std::string executed;
    std::string line1 = "Starts here";
    manager.evaluate(StringView{line1}, [&](const std::string &cmd) { executed = cmd; });
    QCOMPARE(executed, std::string("cmd Starts here"));

    executed = "";
    std::string line2 = "This Ends";
    manager.evaluate(StringView{line2}, [&](const std::string &cmd) { executed = cmd; });
    QCOMPARE(executed, std::string("finish This Ends"));
}

void TestUserActionManager::testSerialization()
{
    setConfig().actions.setData(QVariantMap());
    UserActionManager manager;
    std::ignore = manager.setAction(UserActionType::Regex, "Pattern", "Command");

    auto actions = manager.getAllActions();
    QCOMPARE(actions.size(), size_t(1));
    QCOMPARE(actions[0]->pattern, std::string("Pattern"));
    QCOMPARE(actions[0]->command, std::string("Command"));

    // Check configuration data
    QVariantMap data = getConfig().actions.data();
    QVERIFY(data.contains("Pattern"));
    QCOMPARE(data["Pattern"].toString(), QString("regex:Command"));
}

void TestUserActionManager::testActionRemoval()
{
    setConfig().actions.setData(QVariantMap());
    UserActionManager manager;
    std::ignore = manager.setAction(UserActionType::StartsWith, "Pattern", "Command");
    QCOMPARE(manager.getAllActions().size(), size_t(1));

    std::ignore = manager.removeAction("Pattern");
    QCOMPARE(manager.getAllActions().size(), size_t(0));
}

QTEST_MAIN(TestUserActionManager)
