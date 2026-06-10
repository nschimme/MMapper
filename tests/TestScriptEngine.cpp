// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/parser/ScriptEngine.h"

#include <QtTest>

class TestScriptEngine : public QObject
{
    Q_OBJECT

private slots:
    void testVariableExpansion()
    {
        ScriptEngine engine;
        engine.setVariable("myvar", "hello");
        QCOMPARE(engine.expandVariables("Testing $myvar"), std::string("Testing hello"));
        QCOMPARE(engine.expandVariables("Testing $nonexistent"),
                 std::string("Testing $nonexistent"));
    }

    void testAliasInterception()
    {
        ScriptEngine engine;
        engine.setAlias("k", "kill %1");
        std::string input = "k orc";
        QVERIFY(engine.processUserInput(input));
        QCOMPARE(input, std::string("kill orc"));
    }

    void testActionTrigger()
    {
        ScriptEngine engine;
        std::string lastCmd;
        engine.setExecuteCallback([&](const std::string &cmd) { lastCmd = cmd; });

        std::string line = "You are hungry.";
        engine.setAction(line, "eat bread");
        engine.processServerFeed(StringView(line));
        QCOMPARE(lastCmd, std::string("eat bread"));
    }

    void testExpressionEvaluation()
    {
        ScriptEngine engine;
        engine.setVariable("hp", "50");
        QVERIFY(engine.evaluateExpression("$hp < 100"));
        QVERIFY(!engine.evaluateExpression("$hp > 100"));
        QVERIFY(engine.evaluateExpression("50 == 50"));
    }

    void testMultiCommandExecution()
    {
        ScriptEngine engine;
        std::vector<std::string> cmds;
        engine.setExecuteCallback([&](const std::string &cmd) { cmds.push_back(cmd); });

        engine.executeScript("north; south; #VAR {x} {1}");
        QCOMPARE(cmds.size(), (size_t) 3);
        QCOMPARE(cmds[0], std::string("north"));
        QCOMPARE(cmds[1], std::string("south"));
        QCOMPARE(cmds[2], std::string("#VAR {x} {1}"));
    }
};

QTEST_MAIN(TestScriptEngine)
#include "TestScriptEngine.moc"
