#include "vcodeblockhighlighthelper.h"

#include <QDebug>
#include "vdocument.h"

VCodeBlockHighlightHelper::VCodeBlockHighlightHelper(HGMarkdownHighlighter *p_highlighter,
                                                     VDocument *p_vdoc,
                                                     MarkdownConverterType p_type)
    : QObject(p_highlighter), m_highlighter(p_highlighter), m_vdocument(p_vdoc),
      m_type(p_type), m_timeStamp(0)
{
    connect(p_highlighter, &HGMarkdownHighlighter::codeBlocksUpdated,
            this, &VCodeBlockHighlightHelper::handleCodeBlocksUpdated);
    connect(m_vdocument, &VDocument::textHighlighted,
            this, &VCodeBlockHighlightHelper::handleTextHighlightResult);
}

void VCodeBlockHighlightHelper::handleCodeBlocksUpdated(const QList<VCodeBlock> &p_codeBlocks)
{
    int curStamp = m_timeStamp.fetchAndAddRelaxed(1) + 1;
    m_codeBlocks = p_codeBlocks;
    for (int i = 0; i < m_codeBlocks.size(); ++i) {
        const VCodeBlock &item = m_codeBlocks[i];
        qDebug() << "codeblock" << i << item.m_text << item.m_startBlock << curStamp;
        m_vdocument->highlightTextAsync(item.m_text, i, curStamp);
    }
}

void VCodeBlockHighlightHelper::handleTextHighlightResult(const QString &p_html,
                                                          int p_id,
                                                          int p_timeStamp)
{
    int curStamp = m_timeStamp.load();
    // Abandon obsolete result.
    if (curStamp != p_timeStamp) {
        return;
    }
    qDebug() << "highlighted result" << p_html << p_id << p_timeStamp;
    parseHighlightResult(p_timeStamp, p_id, p_html);
}

static void revertEscapedHtml(QString &p_html)
{
    p_html.replace("&gt;", ">").replace("&lt;", "<").replace("&amp;", "&");
}

void VCodeBlockHighlightHelper::parseHighlightResult(int p_timeStamp,
                                                     int p_idx,
                                                     const QString &p_html)
{
    const VCodeBlock &block = m_codeBlocks.at(p_idx);
    int startPos = block.m_startPos;
    QString text = block.m_text;

    QList<HLUnitPos> hlUnits;

    int textIndex = text.indexOf('\n');
    if (textIndex == -1) {
        return;
    }
    ++textIndex;

    bool failed = true;
    QXmlStreamReader xml(p_html);
    if (xml.readNextStartElement()) {
        if (xml.name() != "pre") {
            goto exit;
        }

        if (!xml.readNextStartElement()) {
            goto exit;
        }

        if (xml.name() != "code") {
            goto exit;
        }

        while (xml.readNext()) {
            if (xml.isCharacters()) {
                // Revert the HTML escape to get the length.
                QString tokenStr = xml.text().toString();
                revertEscapedHtml(tokenStr);
                qDebug() << "body" << textIndex << "text:" << tokenStr;
                textIndex += tokenStr.size();
            } else if (xml.isStartElement()) {
                if (xml.name() != "span") {
                    failed = true;
                    goto exit;
                }
                if (!parseSpanElement(xml, startPos, text, textIndex, hlUnits)) {
                    failed = true;
                    goto exit;
                }
                failed = false;
            } else if (xml.isEndElement()) {
                if (xml.name() != "code" && xml.name() != "pre") {
                    failed = true;
                } else {
                    failed = false;
                }
                break;
            } else {
                failed = true;
                goto exit;
            }
        }
    }

exit:
    if (xml.hasError() || failed) {
        qWarning() << "fail to parse highlighted result"
                   << "stamp:" << p_timeStamp << "index:" << p_idx << p_html;
        return;
    }

    // Pass result back to highlighter.
    int curStamp = m_timeStamp.load();
    // Abandon obsolete result.
    if (curStamp != p_timeStamp) {
        return;
    }

    m_highlighter->setCodeBlockHighlights(hlUnits);
}

bool VCodeBlockHighlightHelper::parseSpanElement(QXmlStreamReader &p_xml,
                                                 int p_startPos,
                                                 const QString &p_text,
                                                 int &p_index,
                                                 QList<HLUnitPos> &p_units)
{
    qDebug() << "parse span" << p_startPos << p_index;
    int unitStart = p_index;
    QString style = p_xml.attributes().value("class").toString();
    while (p_xml.readNext()) {
        if (p_xml.isCharacters()) {
            // Revert the HTML escape to get the length.
            QString tokenStr = p_xml.text().toString();
            revertEscapedHtml(tokenStr);
            qDebug() << "span" << unitStart << "text:" << tokenStr;
            p_index += tokenStr.size();
        } else if (p_xml.isStartElement()) {
            if (p_xml.name() != "span") {
                return false;
            }

            // Sub-span.
            if (!parseSpanElement(p_xml, p_startPos, p_text, p_index, p_units)) {
                return false;
            }
        } else if (p_xml.isEndElement()) {
            if (p_xml.name() != "span") {
                return false;
            }

            // Got a complete span.
            HLUnitPos unit(unitStart + p_startPos, p_index - unitStart, style);
            p_units.append(unit);
            qDebug() << "unit:" << unit.m_position << unit.m_length << unit.m_style;

            return true;
        } else {
            return false;
        }
    }
    return false;
}
