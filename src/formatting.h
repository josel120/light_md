#pragma once

#include <QString>
#include <functional>

class QPlainTextEdit;

// The formatting buttons, as plain functions on the writing pane. Each change is one undo step.
// They only edit text: showing the writing pane and giving it focus is up to the window.
namespace Formatting {

// Puts `prefixFor(1)`, `prefixFor(2)`... in front of each selected line (or the cursor's line).
// Blank lines inside a multi-line selection are skipped.
void prefixLines(QPlainTextEdit *editor, const std::function<QString(int)> &prefixFor);

// Turns the selected lines into titles, replacing any title marks they already have.
void setHeading(QPlainTextEdit *editor, const QString &marks);

// Puts `marks` around the selection, or around a sample word that is left selected.
void wrapSelection(QPlainTextEdit *editor, const QString &marks);

// What the user typed in the link box, made into an address; empty when nothing usable was typed.
QString linkAddress(const QString &typed);

// Links the selection (or a selected sample label) to `address`.
void insertLink(QPlainTextEdit *editor, const QString &address);

// Inserts a multi-line element on its own lines, below the current paragraph, with blank lines
// around it. `selectWord`, when given, is left selected so the user can type over it.
void insertBlock(QPlainTextEdit *editor, const QString &text, const QString &selectWord = QString());

// Fences the selected lines as code, or inserts an empty code block.
void insertCodeBlock(QPlainTextEdit *editor);

} // namespace Formatting
