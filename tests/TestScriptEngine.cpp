// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/configuration/configuration.h"
#include "../src/parser/ScriptEngine.h"

#include <QtTest>

class TestScriptEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { setEnteredMain(); }

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
        engine.setAction(line, "eat bread", ScriptActionType::Wildcard);
        engine.processServerFeed(StringView(std::string_view(line)));
        QCOMPARE(lastCmd, std::string("eat bread"));

        // Test %N not persisting across lines
        lastCmd.clear();
        engine.setAction("Test *", "say %1", ScriptActionType::Wildcard);
        engine.processServerFeed(StringView(std::string_view("Test match")));
        QCOMPARE(lastCmd, std::string("say match"));

        lastCmd.clear();
        engine.processServerFeed(StringView(std::string_view("No match here")));
        // executeScript shouldn't even be called, but if it was (e.g. by another action),
        // %1 should be empty.
        engine.executeScript("say %1");
        QCOMPARE(lastCmd, std::string("say "));
    }

    void testActionTypes()
    {
        ScriptEngine engine;
        std::vector<std::string> cmds;
        engine.setExecuteCallback([&](const std::string &cmd) { cmds.push_back(cmd); });

        engine.setAction("start", "one", ScriptActionType::Starts);
        engine.setAction("end", "two", ScriptActionType::Ends);

        engine.processServerFeed(StringView(std::string_view("starting now")));
        QVERIFY(!cmds.empty());
        QCOMPARE(cmds.back(), std::string("one"));

        engine.processServerFeed(StringView(std::string_view("this is the end")));
        QVERIFY(cmds.size() >= 2);
        QCOMPARE(cmds.back(), std::string("two"));
    }

    void testEvaluationOrder()
    {
        ScriptEngine engine;
        std::vector<std::string> cmds;
        engine.setExecuteCallback([&](const std::string &cmd) { cmds.push_back(cmd); });

        engine.setVariable("x", "old");
        engine.executeScript("#VAR {x} {new}; say $x");

        // The #VAR is a primitive and won't be echoed in cmds.
        // But it WILL update the variable before 'say ' is expanded.
        QCOMPARE(cmds.size(), size_t(1));
        QCOMPARE(cmds[0], std::string("say new"));
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
        // #VAR is a primitive, not echoed.
        QCOMPARE(cmds.size(), size_t(2));
        QCOMPARE(cmds[0], std::string("north"));
        QCOMPARE(cmds[1], std::string("south"));
    }
};

QTEST_MAIN(TestScriptEngine)
#include "TestScriptEngine.moc"
