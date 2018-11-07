#include "commands.h"

namespace Zep
{

// Delete Range of chars
ZepCommand_DeleteRange::ZepCommand_DeleteRange(ZepBuffer& buffer, const BufferLocation& start, const BufferLocation& end, const BufferLocation& cursorAfter)
    : ZepCommand(buffer, cursorAfter),
    m_startOffset(start),
    m_endOffset(end)
{
    // No range - this will kill the operation at the Redo
    if (buffer.GetText().empty())
    {
        m_endOffset = m_startOffset;
    }
    else
    {
        m_endOffset = std::min(m_endOffset, long(buffer.GetText().size()));
    }
}

void ZepCommand_DeleteRange::Redo()
{
    if (m_startOffset != m_endOffset)
    {
        m_deleted = std::string(m_buffer.GetText().begin() + m_startOffset,
            m_buffer.GetText().begin() + m_endOffset);

        m_buffer.Delete(m_startOffset, m_endOffset, m_cursorAfter);
    }
}

void ZepCommand_DeleteRange::Undo()
{
    if (m_deleted.empty())
        return;
    m_buffer.Insert(m_startOffset, m_deleted, m_startOffset);
}

// Insert a string
ZepCommand_Insert::ZepCommand_Insert(ZepBuffer& buffer, const BufferLocation& start, const std::string& str, const BufferLocation& cursorAfter)
    : ZepCommand(buffer, cursorAfter),
    m_startOffset(start),
    m_strInsert(str)
{
}

void ZepCommand_Insert::Redo()
{
    bool ret = m_buffer.Insert(m_startOffset, m_strInsert, m_cursorAfter);
    assert(ret);
    if (ret == true)
    {
        m_endOffsetInserted = m_buffer.LocationFromOffset(m_startOffset + long(m_strInsert.size()));
    }
    else
    {
        m_endOffsetInserted = -1;
    }
}

void ZepCommand_Insert::Undo()
{
    if (m_endOffsetInserted != -1)
    {
        m_buffer.Delete(m_startOffset, m_endOffsetInserted, m_startOffset);
    }
}
}