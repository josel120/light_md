#pragma once

#include <QImage>
#include <QMimeData>
#include <QPlainTextEdit>
#include <functional>

// The writing pane. Same as a plain text box, except that pasting (or dropping) a picture
// hands it to `pasteImage` instead of doing nothing.
class MarkdownEditor : public QPlainTextEdit
{
public:
    using QPlainTextEdit::QPlainTextEdit;

    std::function<void(const QImage &)> pasteImage;

protected:
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
