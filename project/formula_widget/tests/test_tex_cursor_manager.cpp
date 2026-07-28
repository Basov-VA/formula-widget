#include "tex_cursor_manager.hpp"
#include <QtTest/QtTest>
#include <iostream>

class TestTexCursorManager : public QObject
{
    Q_OBJECT

private slots:
    // Группа 1: Базовые операции
    void test_setTexString_simple();
    void test_setTexString_with_spaces();
    void test_setTexString_empty();

    // Группа 2: Вставка символов
    void test_insertChar_atEnd();
    void test_insertChar_atStart();
    void test_insertChar_inMiddle();
    void test_insertChar_invalid();
    void test_isAllowedChar();

    // Группа 3: Удаление символов
    void test_deleteBack_middle();
    void test_deleteBack_atStart();
    void test_deleteForward_middle();
    void test_deleteForward_atEnd();
    void test_deleteAll();

    // Группа 4: Позиционирование курсора по глифу
    void test_setCursorFromGlyph_before();
    void test_setCursorFromGlyph_after();
    void test_setCursorFromGlyph_invalid();

    // Группа 5: glyphIndexAtCursor
    void test_glyphIndexAtCursor_start();
    void test_glyphIndexAtCursor_middle();
    void test_glyphIndexAtCursor_end();

    // Группа 6: Комплексные сценарии
    void test_typeFormulaFromScratch();
    void test_insertAndDelete();
    void test_navigateAndType();
    void test_insertWithSpaces();

    // Группа 7: Граничные случаи
    void test_singleChar_backspace();
    void test_singleChar_delete();
    void test_multipleInserts();
    void test_texPositionForGlyph_invalid();

    // Группа 8: TeX-команды
    void test_texCommandDetection();

    // Группа 9: Позиционирование курсора и вставка символов
    void test_cursorPositioning_insertAtCursor();
    void test_cursorMovement_afterInsert();
    void test_insertAtSpecificPositions();
    void test_glyphMapping_afterInsert();
    void test_insertSpecialCharacters();
    void test_cursorBoundaryChecks();
    void test_multipleInsertions_sequential();
    void test_insertWithNavigation();
    void test_insertInvalidCharacters();
    void test_cursorPositionAfterInvalidInsert();

    // Группа 10: TeX команды - предотвращение вставки внутри команд
    void test_texCommandInsertionPrevention();
    void test_texCommandBoundaryDetection();
    void test_insertAfterTexCommand();
    void test_insertBeforeTexCommand();
    void test_texCommandWithBraces();
    void test_multipleTexCommands();
    void test_texCommandAtStart();
    void test_texCommandAtEnd();
    void test_partialTexCommand();
    void test_texCommandWithSpaces();

    // Группа 11: Вставка с позиции курсора (setCursorFromGlyph + insertChar)
    void test_insert_afterGlyph0_simple();
    void test_insert_beforeGlyph0_simple();
    void test_insert_afterLastGlyph();
    void test_insert_beforeLastGlyph();
    void test_insert_middleGlyph_after();
    void test_insert_middleGlyph_before();
    void test_insert_sequential_right();
    void test_insert_sequential_left();
    void test_insert_afterGlyph_thenDelete();
    void test_insert_beforeGlyph_thenDelete();

    // Группа 12: Удаление с позиции курсора (deleteBack/deleteForward)
    void test_deleteBack_afterFirstChar();
    void test_deleteBack_afterLastChar();
    void test_deleteBack_afterMiddleChar();
    void test_deleteForward_beforeFirstChar();
    void test_deleteForward_beforeLastChar();
    void test_deleteForward_beforeMiddleChar();
    void test_deleteBack_texCommand_whole();
    void test_deleteForward_texCommand_whole();
    void test_deleteBack_openBrace();
    void test_deleteForward_closeBrace();

    // Группа 13: Удаление TeX-команд целиком
    void test_deleteBack_frac_command();
    void test_deleteBack_alpha_command();
    void test_deleteBack_sqrt_command();
    void test_deleteForward_frac_command();
    void test_deleteForward_alpha_command();
    void test_deleteBack_command_in_middle();
    void test_deleteForward_command_in_middle();
    void test_deleteBack_brace_sequence();
    void test_deleteForward_brace_sequence();
    void test_delete_entire_formula_backspace();

    // Группа 14: Вставка в формулы с TeX-командами
    void test_insert_after_frac_command();
    void test_insert_before_frac_command();
    void test_insert_between_commands();
    void test_insert_inside_braces_after_open();
    void test_insert_inside_braces_before_close();
    void test_insert_after_caret_token();
    void test_insert_after_underscore_token();
    void test_insert_multiple_after_command();
    void test_insert_then_deleteBack_restores();
    void test_insert_then_deleteForward_restores();

    // Группа 15: Граничные случаи курсора
    void test_cursor_at_start_deleteBack_noop();
    void test_cursor_at_end_deleteForward_noop();
    void test_cursor_snap_inside_command();
    void test_cursor_snap_to_end_of_string();
    void test_glyphCount_after_insert();
    void test_glyphCount_after_delete();
    void test_glyphCount_command_no_glyph();
    void test_cursorPosition_after_multiple_inserts();
    void test_isCursorAtStart_after_setCursorFromGlyph();
    void test_isCursorAtEnd_after_setCursorFromGlyph();
};

// Группа 1: Базовые операции
void TestTexCursorManager::test_setTexString_simple() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a+b");

    QCOMPARE(mgr.texString(), std::string("a+b"));
    QCOMPARE(mgr.glyphCount(), std::size_t(3));  // a, +, b
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));  // в конце по умолчанию

    // Маппинг: glyph[0]→tex[0], glyph[1]→tex[1], glyph[2]→tex[2]
    QCOMPARE(mgr.texPositionForGlyph(0), std::optional<std::size_t>(0));
    QCOMPARE(mgr.texPositionForGlyph(1), std::optional<std::size_t>(1));
    QCOMPARE(mgr.texPositionForGlyph(2), std::optional<std::size_t>(2));
}

void TestTexCursorManager::test_setTexString_with_spaces() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a + b");

    QCOMPARE(mgr.glyphCount(), std::size_t(3));  // пробелы не считаются
    QCOMPARE(mgr.texPositionForGlyph(0), std::optional<std::size_t>(0));  // 'a' на позиции 0
    QCOMPARE(mgr.texPositionForGlyph(1), std::optional<std::size_t>(2));  // '+' на позиции 2
    QCOMPARE(mgr.texPositionForGlyph(2), std::optional<std::size_t>(4));  // 'b' на позиции 4
}

void TestTexCursorManager::test_setTexString_empty() {
    formula::TexCursorManager mgr;
    mgr.setTexString("");

    QCOMPARE(mgr.glyphCount(), std::size_t(0));
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
    QVERIFY(mgr.isCursorAtStart());
    QVERIFY(mgr.isCursorAtEnd());
}

// Группа 2: Вставка символов
void TestTexCursorManager::test_insertChar_atEnd() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    // cursor_pos_ = 2 (конец)

    QVERIFY(mgr.insertChar('c'));
    QCOMPARE(mgr.texString(), std::string("abc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
    QCOMPARE(mgr.glyphCount(), std::size_t(3));
}

void TestTexCursorManager::test_insertChar_atStart() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorPosition(0);

    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_insertChar_inMiddle() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorPosition(1);  // после 'a'

    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a+b"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
}

void TestTexCursorManager::test_insertChar_invalid() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");

    QVERIFY(!mgr.insertChar('\\'));  // backslash не допустим
    QVERIFY(!mgr.insertChar('{'));
    QVERIFY(!mgr.insertChar('^'));
    QCOMPARE(mgr.texString(), std::string("ab"));  // не изменилось
}

void TestTexCursorManager::test_isAllowedChar() {
    // Буквы
    QVERIFY(formula::TexCursorManager::isAllowedChar('a'));
    QVERIFY(formula::TexCursorManager::isAllowedChar('Z'));

    // Цифры
    QVERIFY(formula::TexCursorManager::isAllowedChar('0'));
    QVERIFY(formula::TexCursorManager::isAllowedChar('9'));

    // Операторы
    QVERIFY(formula::TexCursorManager::isAllowedChar('+'));
    QVERIFY(formula::TexCursorManager::isAllowedChar('-'));
    QVERIFY(formula::TexCursorManager::isAllowedChar('*'));
    QVERIFY(formula::TexCursorManager::isAllowedChar('/'));

    // Недопустимые
    QVERIFY(!formula::TexCursorManager::isAllowedChar('\\'));
    QVERIFY(!formula::TexCursorManager::isAllowedChar('{'));
    QVERIFY(!formula::TexCursorManager::isAllowedChar('}'));
    QVERIFY(!formula::TexCursorManager::isAllowedChar('^'));
    QVERIFY(!formula::TexCursorManager::isAllowedChar('_'));
    QVERIFY(!formula::TexCursorManager::isAllowedChar(' '));
}

// Группа 3: Удаление символов
void TestTexCursorManager::test_deleteBack_middle() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorPosition(2);  // после 'b'

    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("ac"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_deleteBack_atStart() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorPosition(0);

    QVERIFY(!mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("abc"));
}

void TestTexCursorManager::test_deleteForward_middle() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorPosition(1);  // после 'a'

    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("ac"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));  // не изменилась
}

void TestTexCursorManager::test_deleteForward_atEnd() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    // cursor_pos_ = 3 (конец)

    QVERIFY(!mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("abc"));
}

void TestTexCursorManager::test_deleteAll() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");

    // Удалить 'b' (Backspace из конца)
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("a"));

    // Удалить 'a' (Backspace из конца)
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string(""));
    QCOMPARE(mgr.glyphCount(), std::size_t(0));

    // Ещё раз Backspace — ничего
    QVERIFY(!mgr.deleteBack());
}

// Группа 4: Позиционирование курсора по глифу
void TestTexCursorManager::test_setCursorFromGlyph_before() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a+b");

    mgr.setCursorFromGlyph(1, false);  // перед '+'
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_setCursorFromGlyph_after() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a+b");

    mgr.setCursorFromGlyph(1, true);  // после '+'
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
}

void TestTexCursorManager::test_setCursorFromGlyph_invalid() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a+b");

    mgr.setCursorFromGlyph(100, false);
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));  // в конце
}

// Группа 5: glyphIndexAtCursor
void TestTexCursorManager::test_glyphIndexAtCursor_start() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorPosition(0);

    auto idx = mgr.glyphIndexAtCursor();
    QVERIFY(idx.has_value());
    QCOMPARE(*idx, std::size_t(0));
}

void TestTexCursorManager::test_glyphIndexAtCursor_middle() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorPosition(1);  // после 'a', перед 'b'

    auto idx = mgr.glyphIndexAtCursor();
    QVERIFY(idx.has_value());
    QCOMPARE(*idx, std::size_t(1));  // глиф 'b'
}

void TestTexCursorManager::test_glyphIndexAtCursor_end() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    // cursor_pos_ = 3 (конец)

    auto idx = mgr.glyphIndexAtCursor();
    QVERIFY(!idx.has_value());  // нет глифа после курсора
    QVERIFY(mgr.isCursorAtEnd());
}

// Группа 6: Комплексные сценарии
void TestTexCursorManager::test_typeFormulaFromScratch() {
    formula::TexCursorManager mgr;
    mgr.setTexString("");

    // Набираем "a+b" посимвольно
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.texString(), std::string("a"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));

    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a+"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));

    QVERIFY(mgr.insertChar('b'));
    QCOMPARE(mgr.texString(), std::string("a+b"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
    QCOMPARE(mgr.glyphCount(), std::size_t(3));
}

void TestTexCursorManager::test_insertAndDelete() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorPosition(1);  // после 'a'

    // Вставить '+' → "a+b"
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a+b"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));

    // Backspace → "ab"
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("ab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_navigateAndType() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");

    // Курсор перед 'b' (glyph 1)
    mgr.setCursorFromGlyph(1, false);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));

    // Вставить 'x' → "axb"
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("axb"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));

    // Глиф перед курсором — 'x' (glyph 1), после — 'b' (glyph 2)
    auto idx = mgr.glyphIndexAtCursor();
    QVERIFY(idx.has_value());
    QCOMPARE(*idx, std::size_t(2));  // 'b' теперь glyph 2
}

void TestTexCursorManager::test_insertWithSpaces() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a + b");

    // Курсор после '+' (glyph 1)
    mgr.setCursorFromGlyph(1, true);
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));  // позиция после '+' в "a + b"

    // Вставить 'x' → "a +x b"
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("a +x b"));
    QCOMPARE(mgr.glyphCount(), std::size_t(4));  // a, +, x, b
}

// Группа 7: Граничные случаи
void TestTexCursorManager::test_singleChar_backspace() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a");
    // cursor_pos_ = 1

    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string(""));
    QCOMPARE(mgr.glyphCount(), std::size_t(0));
    QVERIFY(mgr.isCursorAtStart());
    QVERIFY(mgr.isCursorAtEnd());
}

void TestTexCursorManager::test_singleChar_delete() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a");
    mgr.setCursorPosition(0);  // перед 'a'

    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string(""));
}

void TestTexCursorManager::test_multipleInserts() {
    formula::TexCursorManager mgr;
    mgr.setTexString("");

    for (char ch = 'a'; ch <= 'e'; ++ch) {
        QVERIFY(mgr.insertChar(ch));
    }

    QCOMPARE(mgr.texString(), std::string("abcde"));
    QCOMPARE(mgr.glyphCount(), std::size_t(5));
    QCOMPARE(mgr.cursorPosition(), std::size_t(5));
}

void TestTexCursorManager::test_texPositionForGlyph_invalid() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");

    QVERIFY(!mgr.texPositionForGlyph(10).has_value());
}

// Группа 8: Тесты для TeX-команд
void TestTexCursorManager::test_texCommandDetection() {
    formula::TexCursorManager mgr;

    // С новой токен-базированной моделью курсор всегда между токенами,
    // поэтому isInsideTexCommand() всегда возвращает false.
    // Тест проверяет, что setCursorPosition корректно снапится к границам токенов.

    // Тест 1: setCursorPosition внутри команды \frac снапится к концу команды
    mgr.setTexString("\\frac");
    mgr.setCursorPosition(2); // Позиция внутри '\frac' → снапится к концу (pos 5)
    QVERIFY(!mgr.isInsideTexCommand()); // всегда false в новой модели
    QCOMPARE(mgr.cursorPosition(), std::size_t(5)); // после \frac

    // Тест 2: setCursorPosition внутри команды \frac{1}{2}
    mgr.setTexString("\\frac{1}{2}");
    mgr.setCursorPosition(2); // Позиция внутри '\frac' → снапится к концу команды (pos 5)
    QVERIFY(!mgr.isInsideTexCommand());
    QCOMPARE(mgr.cursorPosition(), std::size_t(5)); // после \frac

    // Тест 3: setCursorPosition внутри команды \pi
    mgr.setTexString("2\\pi i");
    mgr.setCursorPosition(3); // Позиция внутри '\pi' → снапится к концу (pos 4)
    QVERIFY(!mgr.isInsideTexCommand());
    QCOMPARE(mgr.cursorPosition(), std::size_t(4)); // после \pi

    // Тест 4: setCursorPosition перед командой
    mgr.setTexString("2\\pi i");
    mgr.setCursorPosition(1); // Позиция после '2', перед '\pi'
    QVERIFY(!mgr.isInsideTexCommand());
    QCOMPARE(mgr.cursorPosition(), std::size_t(1)); // после '2'

    // Тест 5: setCursorPosition после команды
    mgr.setTexString("2\\pi i");
    mgr.setCursorPosition(4); // Позиция после '\pi'
    QVERIFY(!mgr.isInsideTexCommand());
    QCOMPARE(mgr.cursorPosition(), std::size_t(4)); // после \pi

    // Тест 6: setCursorPosition внутри длинной команды
    mgr.setTexString("\\mathbb{R}");
    mgr.setCursorPosition(3); // Позиция внутри '\mathbb' → снапится к концу (pos 7)
    QVERIFY(!mgr.isInsideTexCommand());
    QCOMPARE(mgr.cursorPosition(), std::size_t(7)); // после \mathbb

    // Тест 7: setCursorPosition внутри команды в конце строки
    mgr.setTexString("\\mathbb");
    mgr.setCursorPosition(3); // Позиция внутри '\mathbb' → снапится к концу (pos 7)
    QVERIFY(!mgr.isInsideTexCommand());
    QCOMPARE(mgr.cursorPosition(), std::size_t(7)); // после \mathbb
}

// Группа 9: Тесты позиционирования курсора и вставки символов
void TestTexCursorManager::test_cursorPositioning_insertAtCursor() {
    formula::TexCursorManager mgr;

    // Тест 1: Вставка в начало пустой строки
    mgr.setTexString("");
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.texString(), std::string("a"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));

    // Тест 2: Вставка в конец строки
    mgr.setTexString("abc");
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
    QVERIFY(mgr.insertChar('d'));
    QCOMPARE(mgr.texString(), std::string("abcd"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(4));

    // Тест 3: Вставка в середину строки
    mgr.setTexString("abc");
    mgr.setCursorPosition(1); // между 'a' и 'b'
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("axbc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));

    // Тест 4: Вставка перед первым символом
    mgr.setTexString("abc");
    mgr.setCursorPosition(0); // перед 'a'
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xabc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));

    // Тест 5: Вставка между пробелами
    mgr.setTexString("a b");
    mgr.setCursorPosition(2); // между пробелом и 'b'
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("a xb"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));

    // Тест 6: setCursorPosition внутри TeX команды снапится к концу команды
    // В новой токен-базированной модели курсор всегда между токенами
    // При вставке буквы после команды автоматически добавляется пробел-разделитель
    mgr.setTexString("\\pi");
    mgr.setCursorPosition(2); // внутри \pi → снапится к концу (pos 3)
    QCOMPARE(mgr.cursorPosition(), std::size_t(3)); // после \pi
    QVERIFY(mgr.insertChar('x')); // вставка после команды: добавляет пробел → \pi x
    QCOMPARE(mgr.texString(), std::string("\\pi x"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(5));

    // Тест 7: Вставка после TeX команды
    mgr.setTexString("2\\pi");
    mgr.setCursorPosition(4); // после \pi
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("2\\pi x"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(6));

    // Тест 8: Вставка перед TeX командой
    mgr.setTexString("\\pi");
    mgr.setCursorPosition(0); // перед \pi
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("x\\pi"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));

    // Тест 9: Вставка в сложной формуле
    mgr.setTexString("\\frac{1}{2}");
    // Debug: проверим базовые вещи
    QCOMPARE(mgr.texString(), std::string("\\frac{1}{2}"));
    QCOMPARE(mgr.texString().size(), std::size_t(11));
    QCOMPARE(mgr.cursorPosition(), std::size_t(11)); // конец строки

    // Установим курсор на позицию 6
    mgr.setCursorPosition(6);
    QCOMPARE(mgr.cursorPosition(), std::size_t(6));

    // Вставим символ и посмотрим что получится
    QVERIFY(mgr.insertChar('x'));


    QCOMPARE(mgr.texString(), std::string("\\frac{x1}{2}"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(7));

    // Тест 10: Вставка с пробелами в формуле
    mgr.setTexString("a + b");
    mgr.setCursorPosition(3); // между '+' и пробелом
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("a +x b"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(4));
}

void TestTexCursorManager::test_cursorMovement_afterInsert() {
    formula::TexCursorManager mgr;

    // Тест: Курсор перемещается после вставки
    mgr.setTexString("abc");
    mgr.setCursorPosition(1);
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2)); // после вставленного 'x'

    // Вставка еще одного символа
    QVERIFY(mgr.insertChar('y'));
    QCOMPARE(mgr.texString(), std::string("axybc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
}

void TestTexCursorManager::test_insertAtSpecificPositions() {
    formula::TexCursorManager mgr;

    // Тест 1: Вставка на каждой позиции в строке
    mgr.setTexString("123");

    // Позиция 0: перед '1'
    mgr.setCursorPosition(0);
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.texString(), std::string("a123"));

    // Позиция 1: между 'a' и '1'
    mgr.setCursorPosition(1);
    QVERIFY(mgr.insertChar('b'));
    QCOMPARE(mgr.texString(), std::string("ab123"));

    // Позиция 3: между '1' и '2' (после 'ab1')
    mgr.setCursorPosition(3);
    QVERIFY(mgr.insertChar('c'));
    QCOMPARE(mgr.texString(), std::string("ab1c23"));

    // Позиция 6: в конце
    mgr.setCursorPosition(6);
    QVERIFY(mgr.insertChar('d'));
    QCOMPARE(mgr.texString(), std::string("ab1c23d"));
}

void TestTexCursorManager::test_glyphMapping_afterInsert() {
    formula::TexCursorManager mgr;

    // Тест: Маппинг глифов корректно обновляется после вставки
    mgr.setTexString("a b");
    QCOMPARE(mgr.glyphCount(), std::size_t(2)); // 'a' и 'b'

    // Вставка символа между пробелами
    mgr.setCursorPosition(2);
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.glyphCount(), std::size_t(3)); // 'a', 'x', 'b'

    // Проверка позиций глифов
    QCOMPARE(mgr.texPositionForGlyph(0), std::optional<std::size_t>(0)); // 'a'
    QCOMPARE(mgr.texPositionForGlyph(1), std::optional<std::size_t>(2)); // 'x'
    QCOMPARE(mgr.texPositionForGlyph(2), std::optional<std::size_t>(3)); // 'b'
}

void TestTexCursorManager::test_insertSpecialCharacters() {
    formula::TexCursorManager mgr;

    // Тест: Вставка специальных символов
    mgr.setTexString("a");

    // Вставка операторов
    QVERIFY(mgr.insertChar('+'));
    QVERIFY(mgr.insertChar('-'));
    QVERIFY(mgr.insertChar('*'));
    QVERIFY(mgr.insertChar('/'));

    QCOMPARE(mgr.texString(), std::string("a+-*/"));
    QCOMPARE(mgr.glyphCount(), std::size_t(5));
}

void TestTexCursorManager::test_cursorBoundaryChecks() {
    formula::TexCursorManager mgr;

    // Тест: Граничные проверки курсора
    mgr.setTexString("abc");

    // Установка курсора за пределами строки (должно ограничиться размером строки)
    mgr.setCursorPosition(10);
    QCOMPARE(mgr.cursorPosition(), std::size_t(3)); // ограничено до конца строки

    // Вставка в конце
    QVERIFY(mgr.insertChar('d'));
    QCOMPARE(mgr.texString(), std::string("abcd"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(4));

    // Установка курсора в начало
    mgr.setCursorPosition(0);
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));

    // Вставка в начало
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xabcd"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_multipleInsertions_sequential() {
    formula::TexCursorManager mgr;

    // Тест: Последовательные вставки
    mgr.setTexString("");

    // Вставка нескольких символов подряд
    QVERIFY(mgr.insertChar('h'));
    QVERIFY(mgr.insertChar('e'));
    QVERIFY(mgr.insertChar('l'));
    QVERIFY(mgr.insertChar('l'));
    QVERIFY(mgr.insertChar('o'));

    QCOMPARE(mgr.texString(), std::string("hello"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(5));
    QCOMPARE(mgr.glyphCount(), std::size_t(5));
}

void TestTexCursorManager::test_insertWithNavigation() {
    formula::TexCursorManager mgr;

    // Тест: Вставка с навигацией курсора
    mgr.setTexString("abc");

    // Вставка в конец
    QVERIFY(mgr.insertChar('d'));
    QCOMPARE(mgr.texString(), std::string("abcd"));

    // Перемещение курсора в начало
    mgr.setCursorPosition(0);

    // Вставка в начало
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xabcd"));

    // Перемещение курсора в середину
    mgr.setCursorPosition(3);

    // Вставка в середину
    QVERIFY(mgr.insertChar('y'));
    QCOMPARE(mgr.texString(), std::string("xabycd"));
}

void TestTexCursorManager::test_insertInvalidCharacters() {
    formula::TexCursorManager mgr;

    // Тест: Попытка вставки недопустимых символов
    mgr.setTexString("abc");

    // Недопустимые символы
    QVERIFY(!mgr.insertChar(' '));  // пробел
    QVERIFY(!mgr.insertChar('\t')); // табуляция
    QVERIFY(!mgr.insertChar('\n')); // новая строка
    QVERIFY(!mgr.insertChar('@'));  // специальный символ

    // Строка не должна измениться
    QCOMPARE(mgr.texString(), std::string("abc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
}

void TestTexCursorManager::test_cursorPositionAfterInvalidInsert() {
    formula::TexCursorManager mgr;

    // Тест: Позиция курсора после неудачной вставки
    mgr.setTexString("abc");
    mgr.setCursorPosition(1);

    // Попытка вставки недопустимого символа
    QVERIFY(!mgr.insertChar(' '));

    // Курсор должен остаться на той же позиции
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QCOMPARE(mgr.texString(), std::string("abc"));
}

// Группа 10: TeX команды - поведение курсора при позиционировании внутри команд
void TestTexCursorManager::test_texCommandInsertionPrevention() {
    formula::TexCursorManager mgr;

    // В новой токен-базированной модели setCursorPosition внутри команды
    // снапится к ближайшей границе токена (концу команды).
    // Вставка всегда разрешена на границах токенов.
    mgr.setTexString("\\frac{1}{2}");

    // Установить курсор внутри команды \frac (после \f) → снапится к концу \frac (pos 5)
    mgr.setCursorPosition(2); // позиция внутри \frac
    QCOMPARE(mgr.cursorPosition(), std::size_t(5)); // снапилось к концу \frac

    // Вставка после команды: буква → автопробел → \frac a{1}{2}
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.texString(), std::string("\\frac a{1}{2}"));

    // Сброс
    mgr.setTexString("\\frac{1}{2}");

    // Установить курсор внутри команды \frac (после \fr) → снапится к концу \frac (pos 5)
    mgr.setCursorPosition(3); // позиция внутри \frac
    QCOMPARE(mgr.cursorPosition(), std::size_t(5)); // снапилось к концу \frac

    // Вставка после команды: буква → автопробел → \frac x{1}{2}
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("\\frac x{1}{2}"));
}

void TestTexCursorManager::test_texCommandBoundaryDetection() {
    formula::TexCursorManager mgr;

    // Тест: Проверка правильного определения границ TeX-команд
    // В новой токен-базированной модели курсор всегда между токенами
    mgr.setTexString("a\\frac{b}{c}d");

    // Курсор перед командой \frac - вставка разрешена
    mgr.setCursorPosition(1); // позиция после 'a'
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a+\\frac{b}{c}d"));

    // Курсор после команды \frac - вставка разрешена
    mgr.setCursorPosition(13); // позиция после '}' (конец команды) - теперь 13 после вставки +
    QVERIFY(mgr.insertChar('*'));
    QCOMPARE(mgr.texString(), std::string("a+\\frac{b}{c}*d"));

    // Курсор внутри команды \frac → снапится к концу команды (pos 7 = после \frac)
    mgr.setCursorPosition(5); // позиция внутри \frac - теперь 5 после вставки +
    QCOMPARE(mgr.cursorPosition(), std::size_t(7)); // снапилось к концу \frac
    QVERIFY(mgr.insertChar('x')); // вставка после команды: автопробел
    QCOMPARE(mgr.texString(), std::string("a+\\frac x{b}{c}*d"));
}

void TestTexCursorManager::test_insertAfterTexCommand() {
    formula::TexCursorManager mgr;

    // Тест: Вставка символа после завершения TeX-команды
    mgr.setTexString("\\sqrt{x}");

    // Установить курсор после команды \sqrt
    mgr.setCursorPosition(8); // позиция после '}' - теперь 8

    // Вставка должна быть разрешена
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("\\sqrt{x}+"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(9));
}

void TestTexCursorManager::test_insertBeforeTexCommand() {
    formula::TexCursorManager mgr;

    // Тест: Вставка символа перед началом TeX-команды
    mgr.setTexString("\\sqrt{x}");

    // Установить курсор перед командой \sqrt
    mgr.setCursorPosition(0); // позиция перед '\\'

    // Вставка должна быть разрешена
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.texString(), std::string("a\\sqrt{x}"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_texCommandWithBraces() {
    formula::TexCursorManager mgr;

    // Тест: Вставка символов в формулах с фигурных скобками
    // В новой токен-базированной модели курсор всегда между токенами
    mgr.setTexString("\\frac{a}{b}");

    // Курсор внутри фигурных скобок (но не внутри команды) - вставка разрешена
    mgr.setCursorPosition(7); // позиция после 'a' (внутри аргумента)
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("\\frac{a+}{b}"));

    // Курсор внутри команды \frac → снапится к концу \frac (pos 5)
    mgr.setCursorPosition(3); // позиция внутри \frac
    QCOMPARE(mgr.cursorPosition(), std::size_t(5)); // снапилось к концу \frac
    QVERIFY(mgr.insertChar('x')); // вставка после команды: автопробел
    QCOMPARE(mgr.texString(), std::string("\\frac x{a+}{b}"));
}

void TestTexCursorManager::test_multipleTexCommands() {
    formula::TexCursorManager mgr;

    // Тест: Вставка символов в формулах с несколькими TeX-командами
    // В новой токен-базированной модели курсор всегда между токенами
    mgr.setTexString("\\sqrt{x}+\\frac{y}{z}");

    // Вставка между командами - разрешена
    mgr.setCursorPosition(8); // позиция после '}' от \sqrt
    QVERIFY(mgr.insertChar('*'));
    QCOMPARE(mgr.texString(), std::string("\\sqrt{x}*+\\frac{y}{z}"));

    // Курсор внутри второй команды \frac → снапится к концу \frac (pos 15)
    mgr.setCursorPosition(13); // позиция внутри \frac - теперь 13 после вставки *
    QCOMPARE(mgr.cursorPosition(), std::size_t(15)); // снапилось к концу \frac
    QVERIFY(mgr.insertChar('x')); // вставка после команды: автопробел
    QCOMPARE(mgr.texString(), std::string("\\sqrt{x}*+\\frac x{y}{z}"));
}

void TestTexCursorManager::test_texCommandAtStart() {
    formula::TexCursorManager mgr;

    // Тест: TeX-команда в начале строки
    // "\\alpha beta" → tokens: \alpha(0,6), ' '(6,1), b(7,1), e(8,1), t(9,1), a(10,1)
    mgr.setTexString("\\alpha beta");

    // setCursorPosition(3): \alpha.start=0 < 3, next token ' '.start=6 >= 3
    // → cursor_token_pos_=1, cursorPosition()=6
    // Вставка разрешена (курсор снапится к позиции 6, после \alpha)
    mgr.setCursorPosition(3);
    QCOMPARE(mgr.cursorPosition(), std::size_t(6)); // снапилось к позиции после \alpha
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("\\alpha x beta")); // вставка после \alpha: автопробел

    // Сброс
    mgr.setTexString("\\alpha beta");

    // Вставка после команды \alpha - разрешена
    mgr.setCursorPosition(6); // позиция после 'a' (конец команды) и перед пробелом
    QCOMPARE(mgr.cursorPosition(), std::size_t(6)); // курсор точно на границе токена
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("\\alpha+ beta"));
}

void TestTexCursorManager::test_texCommandAtEnd() {
    formula::TexCursorManager mgr;

    // Тест: TeX-команда в конце строки
    // "x+\\beta" → tokens: x(0,1), +(1,1), \beta(2,5)
    mgr.setTexString("x+\\beta");

    // setCursorPosition(4): x.start=0<4, +.start=1<4, \beta.start=2<4, no more tokens
    // → cursor_token_pos_=3 (end), cursorPosition()=7 (string end)
    // Вставка разрешена (курсор снапится к концу строки)
    mgr.setCursorPosition(4);
    QCOMPARE(mgr.cursorPosition(), std::size_t(7)); // снапилось к концу строки
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("x+\\beta x")); // вставка в конец: автопробел

    // Сброс
    mgr.setTexString("x+\\beta");

    // Вставка после команды \beta - разрешена
    mgr.setCursorPosition(7); // позиция после 'a' (конец команды)
    QCOMPARE(mgr.cursorPosition(), std::size_t(7)); // курсор в конце строки
    QVERIFY(mgr.insertChar('*'));
    QCOMPARE(mgr.texString(), std::string("x+\\beta*"));
}

void TestTexCursorManager::test_partialTexCommand() {
    formula::TexCursorManager mgr;

    // Тест: Частичная TeX-команда (незавершенная)
    // "\\fra" → tokens: \fra(0,4) — это команда
    mgr.setTexString("\\fra");

    // setCursorPosition(3): \fra.start=0 < 3, no more tokens
    // → cursor_token_pos_=1 (end), cursorPosition()=4 (string end)
    // Вставка буквы после команды \fra → автопробел → "\fra c"
    mgr.setCursorPosition(3);
    QCOMPARE(mgr.cursorPosition(), std::size_t(4)); // снапилось к концу строки
    QVERIFY(mgr.insertChar('c'));
    QCOMPARE(mgr.texString(), std::string("\\fra c")); // автопробел перед буквой

    // Сброс
    mgr.setTexString("\\fra");

    // Вставка после незавершенной команды - разрешена
    mgr.setCursorPosition(4); // позиция после 'a'
    QCOMPARE(mgr.cursorPosition(), std::size_t(4)); // курсор в конце строки
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("\\fra+"));
}

void TestTexCursorManager::test_texCommandWithSpaces() {
    formula::TexCursorManager mgr;

    // Тест: TeX-команды с пробелами вокруг
    // "a \\frac {b} {c} d" → tokens:
    //   a(0,1), ' '(1,1), \frac(2,5), ' '(7,1), {(8,1), b(9,1), }(10,1),
    //   ' '(11,1), {(12,1), c(13,1), }(14,1), ' '(15,1), d(16,1)
    mgr.setTexString("a \\frac {b} {c} d");

    // setCursorPosition(4): a.start=0<4, ' '.start=1<4, \frac.start=2<4,
    //   next token ' '.start=7 >= 4 → cursor_token_pos_=3, cursorPosition()=7
    // Вставка разрешена (курсор снапится к позиции 7, после \frac)
    mgr.setCursorPosition(4);
    QCOMPARE(mgr.cursorPosition(), std::size_t(7)); // снапилось к позиции после \frac
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("a \\frac x {b} {c} d")); // вставка после \frac: автопробел

    // Сброс
    mgr.setTexString("a \\frac {b} {c} d");

    // Вставка после '}' (позиция 15) - разрешена
    mgr.setCursorPosition(15); // позиция после '}' (конец команды) - теперь 15
    QCOMPARE(mgr.cursorPosition(), std::size_t(15)); // курсор точно на границе токена
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a \\frac {b} {c}+ d"));
}



// =============================================================================
// Группа 11: Вставка с позиции курсора (setCursorFromGlyph + insertChar)
// =============================================================================

void TestTexCursorManager::test_insert_afterGlyph0_simple() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorFromGlyph(0, true);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("axb"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
}

void TestTexCursorManager::test_insert_beforeGlyph0_simple() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorFromGlyph(0, false);
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_insert_afterLastGlyph() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(2, true);
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
    QVERIFY(mgr.insertChar('z'));
    QCOMPARE(mgr.texString(), std::string("abcz"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(4));
}

void TestTexCursorManager::test_insert_beforeLastGlyph() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(2, false);
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
    QVERIFY(mgr.insertChar('z'));
    QCOMPARE(mgr.texString(), std::string("abzc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
}

void TestTexCursorManager::test_insert_middleGlyph_after() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(1, true);
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("abxc"));
}

void TestTexCursorManager::test_insert_middleGlyph_before() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(1, false);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("axbc"));
}

void TestTexCursorManager::test_insert_sequential_right() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a");
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(mgr.insertChar('b'));
    QCOMPARE(mgr.texString(), std::string("ab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
    QVERIFY(mgr.insertChar('c'));
    QCOMPARE(mgr.texString(), std::string("abc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
}

void TestTexCursorManager::test_insert_sequential_left() {
    formula::TexCursorManager mgr;
    mgr.setTexString("c");
    mgr.setCursorFromGlyph(0, false);
    QVERIFY(mgr.insertChar('b'));
    QCOMPARE(mgr.texString(), std::string("bc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    mgr.setCursorFromGlyph(0, false);
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.texString(), std::string("abc"));
}

void TestTexCursorManager::test_insert_afterGlyph_thenDelete() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("axb"));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("ab"));
}

void TestTexCursorManager::test_insert_beforeGlyph_thenDelete() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorFromGlyph(0, false);
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xab"));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("xb"));
}

// =============================================================================
// Группа 12: Удаление с позиции курсора
// =============================================================================

void TestTexCursorManager::test_deleteBack_afterFirstChar() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("bc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
}

void TestTexCursorManager::test_deleteBack_afterLastChar() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(2, true);
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("ab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
}

void TestTexCursorManager::test_deleteBack_afterMiddleChar() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abcd");
    mgr.setCursorFromGlyph(1, true);
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("acd"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_deleteForward_beforeFirstChar() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(0, false);
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("bc"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
}

void TestTexCursorManager::test_deleteForward_beforeLastChar() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(2, false);
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("ab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
}

void TestTexCursorManager::test_deleteForward_beforeMiddleChar() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abcd");
    mgr.setCursorFromGlyph(1, false);
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("acd"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_deleteBack_texCommand_whole() {
    // "a\\frac" → glyph 0='a'; after=true → deleteBack удаляет 'a'
    formula::TexCursorManager mgr;
    mgr.setTexString("a\\frac");
    QCOMPARE(mgr.glyphCount(), std::size_t(1));
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("\\frac"));
}

void TestTexCursorManager::test_deleteForward_texCommand_whole() {
    // "\\frac+b" → tokens: \frac(0,5), +(5,1), b(6,1)
    // glyph 0='+', glyph 1='b'; перед '+' → deleteForward удаляет '+'
    // Примечание: "\\fracb" токенизируется как одна команда \fracb (жадный парсер букв)
    formula::TexCursorManager mgr;
    mgr.setTexString("\\frac+b");
    QCOMPARE(mgr.glyphCount(), std::size_t(2)); // '+' и 'b'
    mgr.setCursorFromGlyph(0, false); // перед '+', cursor_token_pos_=1
    QVERIFY(mgr.deleteForward()); // удаляет '+'
    QCOMPARE(mgr.texString(), std::string("\\fracb"));
}

void TestTexCursorManager::test_deleteBack_openBrace() {
    // "a{b}" → glyph 0='a'; after=true → deleteBack удаляет 'a'
    formula::TexCursorManager mgr;
    mgr.setTexString("a{b}");
    QCOMPARE(mgr.glyphCount(), std::size_t(2));
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("{b}"));
}

void TestTexCursorManager::test_deleteForward_closeBrace() {
    // "a{b}" → glyph 1='b'; after=true → cursor перед '}' → deleteForward удаляет '}'
    formula::TexCursorManager mgr;
    mgr.setTexString("a{b}");
    mgr.setCursorFromGlyph(1, true);
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("a{b"));
}

// =============================================================================
// Группа 13: Удаление TeX-команд целиком
// =============================================================================

void TestTexCursorManager::test_deleteBack_frac_command() {
    // "a\\frac{b}{c}" → setCursorPosition(6) → deleteBack → "a{b}{c}"
    formula::TexCursorManager mgr;
    mgr.setTexString("a\\frac{b}{c}");
    mgr.setCursorPosition(6);
    QCOMPARE(mgr.cursorPosition(), std::size_t(6));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("a{b}{c}"));
}

void TestTexCursorManager::test_deleteBack_alpha_command() {
    // "\\alpha+b" → setCursorPosition(6) → deleteBack → "+b"
    formula::TexCursorManager mgr;
    mgr.setTexString("\\alpha+b");
    mgr.setCursorPosition(6);
    QCOMPARE(mgr.cursorPosition(), std::size_t(6));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("+b"));
}

void TestTexCursorManager::test_deleteBack_sqrt_command() {
    // "\\sqrt{x}" → setCursorPosition(5) → deleteBack → "{x}"
    formula::TexCursorManager mgr;
    mgr.setTexString("\\sqrt{x}");
    mgr.setCursorPosition(5);
    QCOMPARE(mgr.cursorPosition(), std::size_t(5));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("{x}"));
}

void TestTexCursorManager::test_deleteForward_frac_command() {
    // "a\\frac{b}{c}" → setCursorPosition(1) → deleteForward → "a{b}{c}"
    formula::TexCursorManager mgr;
    mgr.setTexString("a\\frac{b}{c}");
    mgr.setCursorPosition(1);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("a{b}{c}"));
}

void TestTexCursorManager::test_deleteForward_alpha_command() {
    // "\\alpha+b" → setCursorPosition(0) → deleteForward → "+b"
    formula::TexCursorManager mgr;
    mgr.setTexString("\\alpha+b");
    mgr.setCursorPosition(0);
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("+b"));
}

void TestTexCursorManager::test_deleteBack_command_in_middle() {
    // "a\\beta c" tokens: a(0,1), \beta(1,5), ' '(6,1), c(7,1)
    formula::TexCursorManager mgr;
    mgr.setTexString("a\\beta c");
    mgr.setCursorPosition(6);
    QCOMPARE(mgr.cursorPosition(), std::size_t(6));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("a c"));
}

void TestTexCursorManager::test_deleteForward_command_in_middle() {
    formula::TexCursorManager mgr;
    mgr.setTexString("a\\beta c");
    mgr.setCursorPosition(1);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("a c"));
}

void TestTexCursorManager::test_deleteBack_brace_sequence() {
    formula::TexCursorManager mgr;
    mgr.setTexString("{a}");
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("{a"));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("{"));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string(""));
}

void TestTexCursorManager::test_deleteForward_brace_sequence() {
    formula::TexCursorManager mgr;
    mgr.setTexString("{a}");
    mgr.setCursorPosition(0);
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("a}"));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("}"));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string(""));
}

void TestTexCursorManager::test_delete_entire_formula_backspace() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("ab"));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("a"));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string(""));
    QVERIFY(!mgr.deleteBack());
}

// =============================================================================
// Группа 14: Вставка в формулы с TeX-командами
// =============================================================================

void TestTexCursorManager::test_insert_after_frac_command() {
    // "\\frac{a}{b}" → setCursorPosition(5) → insert '+' → "\\frac+{a}{b}"
    formula::TexCursorManager mgr;
    mgr.setTexString("\\frac{a}{b}");
    mgr.setCursorPosition(5);
    QCOMPARE(mgr.cursorPosition(), std::size_t(5));
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("\\frac+{a}{b}"));
}

void TestTexCursorManager::test_insert_before_frac_command() {
    formula::TexCursorManager mgr;
    mgr.setTexString("\\frac{a}{b}");
    mgr.setCursorPosition(0);
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("x\\frac{a}{b}"));
}

void TestTexCursorManager::test_insert_between_commands() {
    // "\\alpha\\beta" tokens: \alpha(0,6), \beta(6,5)
    formula::TexCursorManager mgr;
    mgr.setTexString("\\alpha\\beta");
    mgr.setCursorPosition(6);
    QCOMPARE(mgr.cursorPosition(), std::size_t(6));
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("\\alpha+\\beta"));
}

void TestTexCursorManager::test_insert_inside_braces_after_open() {
    // "{a}" tokens: {(0,1), a(1,1), }(2,1)
    formula::TexCursorManager mgr;
    mgr.setTexString("{a}");
    mgr.setCursorPosition(1);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("{xa}"));
}

void TestTexCursorManager::test_insert_inside_braces_before_close() {
    formula::TexCursorManager mgr;
    mgr.setTexString("{a}");
    mgr.setCursorPosition(2);
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("{ax}"));
}

void TestTexCursorManager::test_insert_after_caret_token() {
    // "a^b" tokens: a(0,1), ^(1,1), b(2,1)
    formula::TexCursorManager mgr;
    mgr.setTexString("a^b");
    mgr.setCursorPosition(1);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a+^b"));
}

void TestTexCursorManager::test_insert_after_underscore_token() {
    // "a_b" tokens: a(0,1), _(1,1), b(2,1)
    formula::TexCursorManager mgr;
    mgr.setTexString("a_b");
    mgr.setCursorPosition(1);
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.insertChar('+'));
    QCOMPARE(mgr.texString(), std::string("a+_b"));
}

void TestTexCursorManager::test_insert_multiple_after_command() {
    formula::TexCursorManager mgr;
    mgr.setTexString("\\alpha");
    mgr.setCursorPosition(6);
    // Первая вставка: буква после команды → автопробел → \alpha x
    // Вторая и третья: буква после буквы → без пробела → \alpha xyz
    QVERIFY(mgr.insertChar('x'));
    QVERIFY(mgr.insertChar('y'));
    QVERIFY(mgr.insertChar('z'));
    QCOMPARE(mgr.texString(), std::string("\\alpha xyz"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(10));
}

void TestTexCursorManager::test_insert_then_deleteBack_restores() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("axb"));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("ab"));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
}

void TestTexCursorManager::test_insert_then_deleteForward_restores() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorFromGlyph(0, false);
    QVERIFY(mgr.insertChar('x'));
    QCOMPARE(mgr.texString(), std::string("xab"));
    QVERIFY(mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("xb"));
}

// =============================================================================
// Группа 15: Граничные случаи курсора
// =============================================================================

void TestTexCursorManager::test_cursor_at_start_deleteBack_noop() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorPosition(0);
    QCOMPARE(mgr.cursorPosition(), std::size_t(0));
    QVERIFY(!mgr.deleteBack());
    QCOMPARE(mgr.texString(), std::string("abc"));
}

void TestTexCursorManager::test_cursor_at_end_deleteForward_noop() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
    QVERIFY(!mgr.deleteForward());
    QCOMPARE(mgr.texString(), std::string("abc"));
}

void TestTexCursorManager::test_cursor_snap_inside_command() {
    // "\frac{a}" tokens: \frac(0,5), {(5,1), a(6,1), }(7,1)
    // setCursorPosition(2): \frac.start=0<2, {.start=5>=2 → cursor_token_pos_=1, pos=5
    formula::TexCursorManager mgr;
    mgr.setTexString("\\frac{a}");
    mgr.setCursorPosition(2);
    QCOMPARE(mgr.cursorPosition(), std::size_t(5));
}

void TestTexCursorManager::test_cursor_snap_to_end_of_string() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    mgr.setCursorPosition(100);
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
}

void TestTexCursorManager::test_glyphCount_after_insert() {
    formula::TexCursorManager mgr;
    mgr.setTexString("ab");
    QCOMPARE(mgr.glyphCount(), std::size_t(2));
    mgr.setCursorFromGlyph(1, true);
    QVERIFY(mgr.insertChar('c'));
    QCOMPARE(mgr.glyphCount(), std::size_t(3));
}

void TestTexCursorManager::test_glyphCount_after_delete() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    QCOMPARE(mgr.glyphCount(), std::size_t(3));
    QVERIFY(mgr.deleteBack());
    QCOMPARE(mgr.glyphCount(), std::size_t(2));
}

void TestTexCursorManager::test_glyphCount_command_no_glyph() {
    formula::TexCursorManager mgr;
    mgr.setTexString("\\frac");
    QCOMPARE(mgr.glyphCount(), std::size_t(0));
    mgr.setTexString("\\frac{a}{b}");
    QCOMPARE(mgr.glyphCount(), std::size_t(2));
}

void TestTexCursorManager::test_cursorPosition_after_multiple_inserts() {
    formula::TexCursorManager mgr;
    mgr.setTexString("");
    QVERIFY(mgr.insertChar('a'));
    QCOMPARE(mgr.cursorPosition(), std::size_t(1));
    QVERIFY(mgr.insertChar('b'));
    QCOMPARE(mgr.cursorPosition(), std::size_t(2));
    QVERIFY(mgr.insertChar('c'));
    QCOMPARE(mgr.cursorPosition(), std::size_t(3));
    QCOMPARE(mgr.texString(), std::string("abc"));
}

void TestTexCursorManager::test_isCursorAtStart_after_setCursorFromGlyph() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(0, false);
    QVERIFY(mgr.isCursorAtStart());
    mgr.setCursorFromGlyph(0, true);
    QVERIFY(!mgr.isCursorAtStart());
}

void TestTexCursorManager::test_isCursorAtEnd_after_setCursorFromGlyph() {
    formula::TexCursorManager mgr;
    mgr.setTexString("abc");
    mgr.setCursorFromGlyph(2, true);
    QVERIFY(mgr.isCursorAtEnd());
    mgr.setCursorFromGlyph(2, false);
    QVERIFY(!mgr.isCursorAtEnd());
}

QTEST_MAIN(TestTexCursorManager)
#include "test_tex_cursor_manager.moc"
