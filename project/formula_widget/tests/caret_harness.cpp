// Offscreen-харнесс для ВИЗУАЛЬНОЙ проверки геометрии каретки.
//
// Прогоняет виджет через последовательность нажатий (как реальный ввод),
// затем grab() → PNG. Запускать: QT_QPA_PLATFORM=offscreen ./caret_harness
// из каталога formula_widget/ (нужны шрифты). Картинки в /tmp.

#include "formula_widget.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QPixmap>

#include <string>
#include <vector>

namespace {
    struct Key { int code; QString text; Qt::KeyboardModifiers mods = Qt::NoModifier; };

    void send(FormulaWidget* w, const Key& k) {
        QKeyEvent press(QEvent::KeyPress, k.code, k.mods, k.text);
        QApplication::sendEvent(w, &press);
        QKeyEvent rel(QEvent::KeyRelease, k.code, k.mods, QString());
        QApplication::sendEvent(w, &rel);
    }

    Key ch(char c) { return Key{static_cast<int>(QChar(c).toUpper().unicode()), QString(QChar(c))}; }

    void render(const QString& file, const std::vector<Key>& keys, bool bboxes) {
        FormulaWidget w;
        w.setFontSize(40);
        w.setDebugDrawBBoxes(bboxes);
        w.resize(360, 220);
        w.setFormula("");
        for (const auto& k : keys) send(&w, k);
        QPixmap pix = w.grab();
        pix.save(file);
        qInfo() << "saved" << file << "tex=" << w.currentTexFormula();
    }

    void renderFormula(const QString& file, const QString& tex,
                       const std::vector<Key>& keys, bool bboxes) {
        FormulaWidget w;
        w.setFontSize(44);
        w.setDebugDrawBBoxes(bboxes);
        w.resize(420, 220);
        w.setFormula(tex);
        for (const auto& k : keys) send(&w, k);
        QPixmap pix = w.grab();
        pix.save(file);
        qInfo() << "saved" << file;
    }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    const Key slash{Qt::Key_Slash, "/"};
    const Key caret{Qt::Key_AsciiCircum, "^"};
    const Key tab{Qt::Key_Tab, QString()};
    const Key left{Qt::Key_Left, QString()};
    const Key right{Qt::Key_Right, QString()};

    // 1) Пустой знаменатель: набрать a, /, a, Tab -> \frac{a}{}, курсор в пустом знаменателе
    render("/tmp/caret_denom.png", { slash, ch('a'), tab }, true);
    // 2) Пустой верхний индекс: x, ^ -> x^{}, курсор в пустом верхнем индексе
    render("/tmp/caret_sup.png", { ch('x'), caret }, true);
    // 3) Непустое поле (санити): a, /, a, Tab, b -> курсор после b в знаменателе
    render("/tmp/caret_glyph.png", { slash, ch('a'), tab, ch('b') }, false);

    // 3b) Issue 2: выход вправо из знаменателя — каретка должна встать
    //     справа от дроби на основной строке, а не снизу у последнего глифа.
    render("/tmp/caret_exit.png", { slash, ch('1'), tab, ch('2'), right }, false);

    // 4) Проверка центрирования числителя (issue 1): узкий числитель над широким знаменателем
    {
        FormulaWidget w;
        w.setFontSize(44);
        w.resize(520, 240);
        w.setFormula("\\frac{1}{2\\pi i}\\int_\\gamma\\frac{f(z)}{z-a}");
        w.grab().save("/tmp/formula_full.png");
        qInfo() << "saved /tmp/formula_full.png";
    }

    // 4b) Палитра: публичный API вставки по курсору (то, что дёргают кнопки).
    {
        FormulaWidget w;
        w.setFontSize(44);
        w.resize(460, 200);
        w.setFormula("y=");          // курсор в конце
        w.insertSymbol("int");        // кнопка ∫
        w.insertSymbol("alpha");      // кнопка α
        w.insertSqrt();               // кнопка √ (курсор в подкоренное)
        w.insertSymbol("beta");       // β внутрь корня
        w.grab().save("/tmp/palette_insert.png");
        qInfo() << "saved /tmp/palette_insert.png tex=" << w.currentTexFormula();
    }

    // 5) Навигация вокруг интеграла a ∫_γ b (issue): из конца влево.
    //    2×Left от конца → каретка «после γ» (в нижнем индексе), НЕ прыгает к b.
    renderFormula("/tmp/nav_after_gamma.png", "a\\int_\\gamma b", { left, left }, true);
    //    1×Left от конца → каретка у правого края всего интеграла (перед b).
    renderFormula("/tmp/nav_right_of_int.png", "a\\int_\\gamma b", { left }, true);

    // 6) Функция со скобками: insertFunction("sin") → \sin() курсор внутри, печатаем x.
    {
        FormulaWidget w;
        w.setFontSize(44);
        w.resize(420, 180);
        w.setFormula("y=");
        w.insertFunction("sin");
        send(&w, ch('x'));
        w.grab().save("/tmp/func_sin.png");
        qInfo() << "func_sin tex=" << w.currentTexFormula();
    }

    // 7) Конец дефолтной формулы: каретка после z (раньше исчезала из-за \,).
    renderFormula("/tmp/nav_end.png", "\\frac{f(z)}{z-a}\\,dz", {}, false);

    // 8) Плейсхолдеры пустых полей (Photomath-стиль): пустой интеграл и дробь.
    renderFormula("/tmp/ph_int.png", "\\int^{}_{}", {}, false);
    renderFormula("/tmp/ph_frac.png", "\\frac{a}{}", {}, false);
    renderFormula("/tmp/ph_sum.png", "\\sum^{}_{}+x", {}, false);

    // 9) Пустая дробь: должна рисоваться черта + два плейсхолдера.
    renderFormula("/tmp/frac_empty.png", "\\frac{}{}", {}, false);
    // Вставка дроби в пустую формулу на дефолтном кегле (курсор в числителе).
    {
        FormulaWidget w;
        w.setFontSize(28);
        w.resize(300, 220);
        w.setFormula("");
        w.insertFraction();
        w.grab().save("/tmp/frac_inserted.png");
        qInfo() << "frac_inserted tex=" << w.currentTexFormula();
    }
    // Issue 2 (пустая дробь, самая правая): выйти вправо и проверить каретку.
    {
        FormulaWidget w;
        w.setFontSize(46);
        w.resize(300, 220);
        w.setFormula("");
        w.insertFraction();           // пустая дробь, курсор в числителе
        send(&w, right);              // -> знаменатель
        send(&w, right);              // -> выход вправо (после дроби)
        w.grab().save("/tmp/frac_empty_exit.png");
        send(&w, ch('x'));
        qInfo() << "empty-frac-exit tex=" << w.currentTexFormula();
    }

    // Issue 2: дробь — самый правый элемент. Навигация клавишами до позиции
    // ПОСЛЕ дроби: каретка должна быть видна справа от дроби.
    {
        FormulaWidget w;
        w.setFontSize(44);
        w.resize(340, 220);
        w.setFormula("");
        w.insertFraction();                       // курсор в числителе
        send(&w, ch('a')); send(&w, tab); send(&w, ch('b'));  // \frac{a}{b}, курсор в знаменателе
        send(&w, right);                          // выйти вправо (после дроби) — каретка включается
        w.grab().save("/tmp/frac_after.png");
        send(&w, ch('c'));
        qInfo() << "after-frac tex=" << w.currentTexFormula();
    }

    // 10) Два интеграла: курсор в нижнем пределе ПРАВОГО (пустого). Каретка
    //     должна быть у правого интеграла, а НЕ у нижнего предела левого.
    {
        FormulaWidget w;
        w.setFontSize(40);
        w.resize(520, 220);
        w.setFormula("\\int_c^d+");     // левый интеграл с пределами c,d
        w.insertBigOperator("int");      // правый интеграл, курсор в его нижнем пределе
        w.grab().save("/tmp/two_int_down.png");
        qInfo() << "two-int tex=" << w.currentTexFormula();
    }

    return 0;
}
