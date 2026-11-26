#pragma once

#include <QImage>
#include <QMimeData>
#include <QPlainTextEdit>
#include <functional>

class SpellChecker;

// The writing pane. Same as a plain text box, except that pasting (or dropping) a picture
// hands it to `pasteImage` instead of doing nothing, and right-clicking a misspelled word
// offers corrections (editor.cpp).
class MarkdownEditor : public QPlainTextEdit
{
public:
    using QPlainTextEdit::QPlainTextEdit;

    std::function<void(const QImage &)> pasteImage;
    // Suggestions come from `speller`; "Add to dictionary" calls `addWord`.
    const SpellChecker *speller = nullptr;
    std::function<void(const QString &)> addWord;

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

    bool canInsertFromMimeData(const QMimeData *source) const override
    {
        return (pasteImage && source->hasImage()) || QPlainTextEdit::canInsertFromMimeData(source);
    }

    void insertFromMimeData(const QMimeData *source) override
    {
        // Text wins: Word or Excel put a picture of the selection on the clipboard next to the text.
        if (pasteImage && source->hasImage() && source->text().isEmpty()) {
            const QImage image = qvariant_cast<QImage>(source->imageData());
            if (!image.isNull()) {
                pasteImage(image);
                return;
            }
        }
        QPlainTextEdit::insertFromMimeData(source);
    }
};
