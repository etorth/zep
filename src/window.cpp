#include <sstream>
#include <cctype>
#include <cmath>

#include "display.h"
#include "tab_window.h"
#include "window.h"
#include "syntax.h"
#include "buffer.h"
#include "mode.h"
#include "theme.h"

#include "utils/stringutils.h"

// A 'window' is like a vim buffer display; i.e. a region inside a tab - perhaps one of many organised into rects
namespace Zep
{

namespace
{
const uint32_t Color_CursorNormal = 0xEEF35FBC;
const uint32_t Color_CursorInsert = 0xFFFFFFFF;
}

ZepWindow::ZepWindow(ZepTabWindow& window, ZepBuffer* buffer)
    : ZepComponent(window.GetEditor()),
    m_window(window),
    m_pBuffer(buffer)
{
}

ZepWindow::~ZepWindow()
{
}

void ZepWindow::SetCursorMode(CursorMode mode)
{
    cursorMode = mode;
    GetEditor().ResetCursorTimer();
}

void ZepWindow::Notify(std::shared_ptr<ZepMessage> payload)
{
    if (payload->messageId == Msg_Buffer)
    {
        auto pMsg = std::static_pointer_cast<BufferMessage>(payload);

        if (pMsg->pBuffer != m_pBuffer)
        {
            return;
        }

        if (pMsg->type != BufferMessageType::PreBufferChange)
        {
            m_pendingLineUpdate = true;

            // Put the cursor where the replaced text was added
            GetEditor().ResetCursorTimer();
        }
    }
}

NVec2i ZepWindow::ClampToVisible(NVec2i pos) const
{
    // Cursor 0,0 is top left of window
    // Here we make the cursor land on a valid window character
    pos.y = std::max(0l, pos.y);
    pos.x = std::max(0l, pos.x);
    if (!windowLines.empty())
    {
        pos.y = std::min(pos.y, VisibleLineCount() - 1);
        pos.x = std::min(pos.x, GetCursorLineInfo(pos.y).lastNonCROffset - GetCursorLineInfo(pos.y).columnOffsets.x);
    }
    else
    {
        pos.x = 0;
        pos.y = 0;
    }
    
    return pos;
}

NVec2i ZepWindow::BufferToDisplay()
{
    return BufferToDisplay(m_bufferCursor);
}

// TODO: this can be faster.
NVec2i ZepWindow::BufferToDisplay(const BufferLocation& loc)
{
    if (m_pendingLineUpdate)
    {
        UpdateVisibleLineData();
    }

    NVec2i ret(0, 0);
    if (windowLines.empty())
    {
        ret.x = 0;
        ret.y = 0;
        return ret;
    }

    // Only return a valid location if on the current display
    int lineCount = 0;
    for(auto& line : windowLines)  
    {
        if ((line.columnOffsets.x <= loc) &&
            (line.columnOffsets.y > loc))
        {
            ret.y = lineCount;
            ret.x = loc - line.columnOffsets.x;
            break;
        }
        lineCount++;
    }

    return ret;
}

void ZepWindow::MoveCursorInsideLine(LineLocation location)
{
    switch (location)
    {
    case LineLocation::LineBegin:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::BeyondLineEnd:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineCRBegin:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineFirstGraphChar:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineLastGraphChar:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineLastNonCR:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    }

    m_pendingLineUpdate = true;
}

void ZepWindow::MoveCursorTo(BufferLocation location)
{
    m_bufferCursor = m_pBuffer->Clamp(location);
    m_pendingLineUpdate = true;
   
    auto displayCursor = BufferToDisplay();
    auto& line = windowLines[displayCursor.y];
    lastCursorC = displayCursor.x;
}

// Not currently used
std::pair<BufferLocation, BufferLocation> ZepWindow::GetVisibleBufferRange() const
{
    if (windowLines.empty())
    {
        return std::make_pair(BufferLocation(0), BufferLocation(0));
    }
    return std::make_pair(windowLines[visibleLineRange.x].columnOffsets.x, windowLines[visibleLineRange.y - 1].columnOffsets.y);
}

void ZepWindow::ScrollToCursor()
{
    auto cursor = BufferToDisplay();
    bool changed = false;

    // Handle the case where there is no need to scroll, since the visible lines are inside
    // The current screen rectangle.
    if (cursor.y >= visibleLineRange.y &&
        m_linesFillScreen)
    {
        visibleLineRange.x = cursor.y - VisibleLineCount() + 1;
        changed = true;
    }
    else if (cursor.y < visibleLineRange.x)
    {
        visibleLineRange.x = cursor.y;
        changed = true;
    }

    if (changed)
    {
        visibleLineRange.x = std::min((long)windowLines.size() -1, visibleLineRange.x);
        visibleLineRange.x = std::max(0l, (long)visibleLineRange.x);
    }
    UpdateScreenLines();

#if DEBUG_LINES
    GetEditor().SetCommandText(std::string("Line Range: ") + std::to_string(visibleLineRange.x) + ", " + std::to_string(visibleLineRange.y) + ", cursor: " + std::to_string(cursor.y));
#endif
}

void ZepWindow::MoveCursorWindowRelative(int yDistance, LineLocation clampLocation)
{
    auto cursorCL = BufferToDisplay();
    if (cursorCL.x == -1)
        return;

    // Find the screen line relative target
    auto target = cursorCL + NVec2i(0, yDistance);
    target.y = std::max(0l, target.y);
    target.y = std::min(target.y, long(windowLines.size() - 1));
    
    auto& line = windowLines[target.y];
   
    // Snap to the new vertical column if necessary (see comment below)
    if (target.x < lastCursorC)
        target.x = lastCursorC;

    // Update the master buffer cursor
    m_bufferCursor = line.columnOffsets.x + target.x;
  
    // Ensure the current x offset didn't walk us off the line (column offset is 1 beyond, and there is a single \n before it)
    // We are clamping to visible line here
    m_bufferCursor = std::min(m_bufferCursor, line.columnOffsets.y - 2);
    m_bufferCursor = std::max(m_bufferCursor, line.columnOffsets.x);

    GetEditor().ResetCursorTimer();
}

void ZepWindow::SetSelectionRange(BufferLocation start, BufferLocation end)
{
    selection.start = start;
    selection.end = end;
    selection.vertical = false;
    if (selection.start > selection.end)
    {
        std::swap(selection.start, selection.end);
    }
}

void ZepWindow::SetBuffer(ZepBuffer* pBuffer)
{
    assert(pBuffer);
    m_pBuffer = pBuffer;
    m_pendingLineUpdate = true;
}

void ZepWindow::SetStatusText(const std::string& strText)
{
    statusLines = StringUtils::SplitLines(strText);
}

void ZepWindow::PreDisplay(ZepDisplay& display, const DisplayRegion& region)
{
    m_pDisplay = &display;
    m_bufferRegion = region;

    // ** Temporary, status
    std::ostringstream str;
    str << "(" << GetEditor().GetCurrentMode()->Name() << ") NORMAL" << " : " << int(m_pBuffer->GetLineEnds().size()) << " Lines";
    SetStatusText(str.str());

    auto statusCount = statusLines.size();
    const auto windowSize = m_bufferRegion.bottomRightPx - m_bufferRegion.topLeftPx;
    const float statusSize = display.GetFontSize() * statusCount + textBorder * 2.0f;

    m_statusRegion.bottomRightPx = m_bufferRegion.bottomRightPx;
    m_statusRegion.topLeftPx = m_bufferRegion.bottomRightPx - NVec2f(windowSize.x, statusSize);

    auto lastRegion = m_textRegion;
    m_textRegion.bottomRightPx = m_statusRegion.topLeftPx + NVec2f(windowSize.x, 0.0f);
    m_textRegion.topLeftPx = m_bufferRegion.topLeftPx;

    // Border, and move the text across a bit
    m_leftRegion.topLeftPx = m_textRegion.topLeftPx;
    m_leftRegion.bottomRightPx = NVec2f(m_leftRegion.topLeftPx.x + leftBorder, m_textRegion.bottomRightPx.y);

    m_textRegion.topLeftPx.x += leftBorder + textBorder;

    m_defaultLineSize = display.GetTextSize((Zep::utf8*)"A").y;

    // If the text region changes, we need to layout the text again
    if (lastRegion != m_textRegion)
    {
        UpdateVisibleLineData();
    }
    ScrollToCursor();
}

void ZepWindow::UpdateScreenLines()
{
    m_linesFillScreen = false;

    // Update the Y coordinates of the visible lines
    // And update the range of visible lines
    auto posY = m_textRegion.topLeftPx.y;
    auto screenLine = 0;
    long visLine = visibleLineRange.x;
    while (visLine < windowLines.size())
    {
        // Outside of screen space
        if ((posY + m_defaultLineSize) >= m_textRegion.bottomRightPx.y)
        {
            m_linesFillScreen = true;
            break;
        }

        auto& lineInfo = windowLines[visLine++];
        lineInfo.screenPosYPx = posY;
        posY += m_defaultLineSize;
        screenLine++;
    }
    visibleLineRange.y = visLine;
}

void ZepWindow::UpdateVisibleLineData()
{
    m_pendingLineUpdate = false;
    windowLines.clear();

    m_maxDisplayLines = (long)std::max(0.0f, std::floor((m_textRegion.bottomRightPx.y - m_textRegion.topLeftPx.y) / m_defaultLineSize));

    // Collect all display lines.
    // Lines before the first active line will have screen line number -1
    LineInfo lineInfo;
    lineInfo.lastNonCROffset = -1;
    lineInfo.bufferLineNumber = long(0);
    lineInfo.screenPosYPx = 0;

    float screenPosX = m_textRegion.topLeftPx.x;

    BufferLocation loc = 0;

    // Process every buffer line
    for (;;)
    {
        // We haven't processed this line yet, so we can't display anything
        // else
        if (m_pBuffer->GetLineCount() <= lineInfo.bufferLineNumber)
            break;

        NVec2i columnOffsets;
        if (!m_pBuffer->GetLineOffsets(lineInfo.bufferLineNumber, columnOffsets.x, columnOffsets.y))
            break;

        // Start this line at the current offset
        lineInfo.columnOffsets.x = columnOffsets.x;
        lineInfo.columnOffsets.y = columnOffsets.x;

        const auto& textBuffer = m_pBuffer->GetText();

        // Walk from the start of the line to the end of the line (in buffer chars)
        // Line:
        // [beginoffset]ABCDEF\n[endoffset]
        for (auto ch = columnOffsets.x; ch < columnOffsets.y; ch++)
        {
            const utf8* pCh = &textBuffer[ch];

            // Not at the end, keep going with non CR chars
            if (*pCh != '\n' && *pCh != 0)
            {
                lineInfo.lastNonCROffset = std::max(ch, 0l);
            }

            // TODO: Central UTF-8 helpers.  This is mostly broken; zep doesn't do utf8 yet
#define UTF8_CHAR_LEN( byte ) (( 0xE5000000 >> (( byte >> 3 ) & 0x1e )) & 3 ) + 1
            const utf8* pEnd = pCh + UTF8_CHAR_LEN(*pCh);

            // This is the only place we need the display object outside of rendering; perhaps we can 
            // factor it out.
            // Line length calculation for display in a window shouldn't need the display code, but it does
            // need to know about the font...
            auto textSize = m_pDisplay->GetTextSize(pCh, pEnd);

            // The whole column ends after this ch
            lineInfo.columnOffsets.y = ch + 1;

            // Wrap
            if (wrap)
            {
                if (((screenPosX + textSize.x) + textSize.x) >= (m_textRegion.bottomRightPx.x))
                {
                    // Remember the offset beyond the end of hte line
                    windowLines.push_back(lineInfo);

                    // Now jump to the next 'screen line' for the rest of this 'buffer line'
                    lineInfo.columnOffsets = NVec2i(ch, ch + 1);
                    lineInfo.lastNonCROffset = 0;
                    screenPosX = m_textRegion.topLeftPx.x;
                }
                else
                {
                    screenPosX += textSize.x;
                }
            }
        }

        // Remember this window line, and if not yet found all visible, record the last limit
        windowLines.push_back(lineInfo);

        // Next time round
        lineInfo.bufferLineNumber++;
        lineInfo.lastNonCROffset = 0;

        // Wrap to next line
        screenPosX = m_textRegion.topLeftPx.x;
    }

    if (windowLines.empty())
    {
        LineInfo lineInfo;
        lineInfo.columnOffsets.x = 0;
        lineInfo.columnOffsets.y = 0;
        lineInfo.lastNonCROffset = 0;
        lineInfo.bufferLineNumber = 0;
        windowLines.push_back(lineInfo);
    }
}

const LineInfo& ZepWindow::GetCursorLineInfo(long y) const
{
    y = std::max(0l, y);
    y = std::min(y, long(windowLines.size() - 1));
    return windowLines[y];
}

// TODO: This function draws one char at a time.  It could be more optimal at the expense of some 
// complexity.  
// The text is displayed acorrding to the region bounds and the display lineData
// Additionally (and perhaps that should be a seperate function), this code draws line numbers
bool ZepWindow::DisplayLine(ZepDisplay& display, const LineInfo& lineInfo, const DisplayRegion& region, int displayPass)
{
    // A middle-dot whitespace character
    static const auto whiteSpace = StringUtils::makeStr(std::wstring(L"\x00b7"));
    static const auto blankSpace = ' ';

    auto activeWindow = (GetEditor().GetActiveTabWindow()->GetActiveWindow() == this);

    auto cursorCL = BufferToDisplay();

    // Draw line numbers
    auto showLineNumber = [&]()
    {
        if (cursorCL.x == -1)
            return;

        auto cursorBufferLine = GetCursorLineInfo(cursorCL.y).bufferLineNumber;
        std::string strNum;
        if (displayMode == DisplayMode::Vim)
        {
            strNum = std::to_string(std::abs(lineInfo.bufferLineNumber - cursorBufferLine));
        }
        else
        {
            strNum = std::to_string(lineInfo.bufferLineNumber);
        }
        auto textSize = display.GetTextSize((const utf8*)strNum.c_str(), (const utf8*)(strNum.c_str() + strNum.size()));

        // Number background
        display.DrawRectFilled(NVec2f(m_leftRegion.topLeftPx.x, lineInfo.screenPosYPx),
            NVec2f(m_leftRegion.bottomRightPx.x, lineInfo.screenPosYPx + display.GetFontSize()),
            0xFF222222);

        auto digitCol = 0xFF11FF11;
        if (lineInfo.BufferCursorInside(m_bufferCursor))
        {
            digitCol = Color_CursorNormal;
        }

        // Numbers
        display.DrawChars(NVec2f(m_leftRegion.bottomRightPx.x - textSize.x - textBorder, lineInfo.screenPosYPx), digitCol,
            (const utf8*)strNum.c_str(),
            (const utf8*)(strNum.c_str() + strNum.size()));
    };

    if (displayPass == WindowPass::Background)
    {
        showLineNumber();
    }

    bool foundCR = false;
    auto screenPosX = m_textRegion.topLeftPx.x;

    char invalidChar;

    bool foundCursor = false;

    // Walk from the start of the line to the end of the line (in buffer chars)
    for (auto ch = lineInfo.columnOffsets.x; ch <= lineInfo.columnOffsets.y; ch++)
    {
        auto pSyntax = m_pBuffer->GetSyntax();
        auto col = pSyntax != nullptr ? Theme::Instance().GetColor(pSyntax->GetSyntaxAt(ch)) : 0xFFFFFFFF;
        const utf8* pCh = nullptr;
        if (ch < m_pBuffer->GetText().size())
        {
            pCh = &m_pBuffer->GetText()[ch];
        }
        else
        {
            pCh = (const utf8*)&blankSpace;
        }

        auto bufferLocation = ch;

        // Visible white space
        if (pSyntax && pSyntax->GetSyntaxAt(ch) == SyntaxType::Whitespace)
        {
            pCh = (const utf8*)whiteSpace.c_str();
        }

        // Shown only one char for end of line
        if (*pCh == '\n' ||
            *pCh == 0)
        {
            invalidChar = '@' + *pCh;
            if (m_windowFlags & WindowFlags::ShowCR)
            {
                pCh = (const utf8*)&invalidChar;
            }
            else
            {
                pCh = (const utf8*)&blankSpace;
            }
            col = 0x771111FF;
        }

        // TODO: Central UTF-8 helpers
        // TODO: Test UTF/fix it.  I'm keeping UTF in mind but not testing it yet.
#define UTF8_CHAR_LEN( byte ) (( 0xE5000000 >> (( byte >> 3 ) & 0x1e )) & 3 ) + 1
        auto pEnd = pCh + UTF8_CHAR_LEN(*pCh);

        auto textSize = display.GetTextSize(pCh, pEnd);

        if (displayPass == 0)
        {
            if (activeWindow)
            {
                if (cursorMode == CursorMode::Visual)
                {
                    if (bufferLocation >= selection.start &&
                        bufferLocation <= selection.end)
                    {
                        display.DrawRectFilled(NVec2f(screenPosX, lineInfo.screenPosYPx), NVec2f(screenPosX + textSize.x, lineInfo.screenPosYPx + textSize.y), 0xFF784F26);
                    }
                }

                if (lineInfo.BufferCursorInside(m_bufferCursor))
                {
                    if (m_bufferCursor == ch)
                    {
                        // Cursor
                        cursorPosPx = NVec2f(m_textRegion.topLeftPx.x + textSize.x * cursorCL.x, lineInfo.screenPosYPx);
                        auto cursorBlink = GetEditor().GetCursorBlinkState();
                        if (!cursorBlink)
                        {
                            switch (cursorMode)
                            {
                            default:
                            case CursorMode::Hidden:
                                break;

                            case CursorMode::Insert:
                            {
                                display.DrawRectFilled(NVec2f(cursorPosPx.x - 1, cursorPosPx.y), NVec2f(cursorPosPx.x, cursorPosPx.y + textSize.y), 0xEEFFFFFF);
                            }
                            break;

                            case CursorMode::Normal:
                            case CursorMode::Visual:
                            {
                                display.DrawRectFilled(cursorPosPx, NVec2f(cursorPosPx.x + textSize.x, cursorPosPx.y + textSize.y), Color_CursorNormal);
                            }
                            break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (pSyntax && pSyntax->GetSyntaxAt(ch) == SyntaxType::Whitespace)
            {
                auto centerChar = NVec2f(screenPosX + textSize.x / 2, lineInfo.screenPosYPx + textSize.y / 2);
                display.DrawRectFilled(centerChar - NVec2f(1.0f, 1.0f), centerChar + NVec2f(1.0f, 1.0f), 0xFF524814);
            }
            else
            {
                display.DrawChars(NVec2f(screenPosX, lineInfo.screenPosYPx), col,
                    pCh,
                    pEnd);
            }
        }

        screenPosX += textSize.x;
    }

    return true;
}

void ZepWindow::Display(ZepDisplay& display)
{
    PreDisplay(display, m_bufferRegion);

    auto cursorCL = BufferToDisplay(m_bufferCursor);

    auto activeWindow = (GetEditor().GetActiveTabWindow()->GetActiveWindow() == this);
    cursorPosPx = m_bufferRegion.topLeftPx;

    if (activeWindow && cursorCL.x != -1)
    {
        if (cursorMode == CursorMode::Normal ||
            cursorMode == CursorMode::Insert)
        {
            auto& cursorLine = GetCursorLineInfo(cursorCL.y);

            // Cursor line 
            display.DrawRectFilled(NVec2f(m_textRegion.topLeftPx.x, cursorLine.screenPosYPx),
                NVec2f(m_textRegion.bottomRightPx.x, cursorLine.screenPosYPx + display.GetFontSize()),
                0xFF222222);
        }
    }

    for (int displayPass = 0; displayPass < WindowPass::Max; displayPass++)
    {
        for (long windowLine = visibleLineRange.x; windowLine < visibleLineRange.y; windowLine++)
        {
            auto& lineInfo = windowLines[windowLine];
            if (!DisplayLine(display, lineInfo, m_textRegion, displayPass))
            {
                break;
            }
        }
    }

    if (statusLines.empty())
        statusLines.push_back(" ");
    long statusCount = long(statusLines.size());
    const float statusSize = display.GetFontSize() * statusCount + textBorder * 2.0f;
    auto statusSpace = statusCount;
    statusSpace = std::max(statusCount, 0l);

    // Background rect for status (airline)
    display.DrawRectFilled(m_statusRegion.topLeftPx, m_statusRegion.bottomRightPx, 0xAA111111);

    // Draw status text
    NVec2f screenPosYPx = m_statusRegion.topLeftPx + NVec2f(0.0f, textBorder);
    for (int i = 0; i < statusSpace; i++)
    {
        auto textSize = display.GetTextSize((const utf8*)statusLines[i].c_str(),
            (const utf8*)(statusLines[i].c_str() + statusLines[i].size()));

        display.DrawRectFilled(screenPosYPx, screenPosYPx + NVec2f(textSize.x, display.GetFontSize() + textBorder), 0xFF111111 );
        display.DrawChars(screenPosYPx,
            0xFFFFFFFF,
            (const utf8*)(statusLines[i].c_str()));

        screenPosYPx.y += display.GetFontSize();
        screenPosYPx.x = m_statusRegion.topLeftPx.x;
    }

}
} // Zep

