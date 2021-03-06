#include <string>
#include <QKeyEvent>
#include <QApplication>
#include "editor.h"
#include "mode.h"
#include "zepwidget_qt.h"
#include "tab_window.h"
#include "zepdisplay_qt.h"

namespace Zep
{

ZepWidget_Qt::ZepWidget_Qt(QWidget* pParent)
    : QWidget(pParent)
{
    m_spEditor = std::make_unique<ZepEditor>();
    m_spEditor->RegisterCallback(this);

    m_spDisplay = std::make_unique<ZepDisplay_Qt>(*m_spEditor.get());
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);

    m_refreshTimer.setInterval(250);
    m_refreshTimer.setSingleShot(false);
    m_refreshTimer.start();
    connect(&m_refreshTimer, &QTimer::timeout, this, &ZepWidget_Qt::OnTimer);
}

ZepWidget_Qt::~ZepWidget_Qt()
{
    m_spEditor->UnRegisterCallback(this);

    m_spDisplay.reset();
    m_spEditor.reset();
}

void ZepWidget_Qt::Notify(std::shared_ptr<ZepMessage> message)
{
    if (message->messageId == Msg_Quit)
    {
        qApp->quit();
    }
}

void ZepWidget_Qt::OnTimer()
{
    if (m_spEditor->RefreshRequired())
    {
        update();
    }
}

void ZepWidget_Qt::paintEvent(QPaintEvent* pPaint)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);
    painter.fillRect(rect(), QColor::fromRgbF(.1f, .1f, .1f, 1.0f));

    m_spDisplay->SetPainter(&painter);

    m_spDisplay->SetDisplaySize(NVec2f(painter.viewport().left(), painter.viewport().top()),
        NVec2f(painter.viewport().width(), painter.viewport().height()));

    m_spDisplay->Display();

    m_spDisplay->SetPainter(nullptr);
}

void ZepWidget_Qt::keyPressEvent(QKeyEvent* ev)
{
    uint32_t mod = 0;
    auto pMode = m_spEditor->GetCurrentMode();
    if (ev->modifiers() & Qt::ControlModifier)
    {
        if (ev->key() == Qt::Key_1)
        {
            m_spEditor->SetMode(Zep::StandardMode);
        }
        else if (ev->key() == Qt::Key_2)
        {
            m_spEditor->SetMode(Zep::VimMode);
        }
        else
        {
            if (ev->key() >= Qt::Key_A && ev->key() <= Qt::Key_Z)
            {
                pMode->AddKeyPress((ev->key() - Qt::Key_A) + 'a', ModifierKey::Ctrl);
            }
        }
        update();
        return;
    }

    if (ev->key() == Qt::Key_Tab)
    {
        pMode->AddKeyPress(ExtKeys::TAB, mod);
    }
    else if (ev->key() == Qt::Key_Escape)
    {
        pMode->AddKeyPress(ExtKeys::ESCAPE, mod);
    }
    else if (ev->key() == Qt::Key_Enter ||
        ev->key() == Qt::Key_Return)
    {
        pMode->AddKeyPress(ExtKeys::RETURN, mod);
    }
    else if (ev->key() == Qt::Key_Delete)
    {
        pMode->AddKeyPress(ExtKeys::DEL, mod);
    }
    else if (ev->key() == Qt::Key_Backspace)
    {
        pMode->AddKeyPress(ExtKeys::BACKSPACE, mod);
    }
    else if (ev->key() == Qt::Key_Home)
    {
        pMode->AddKeyPress(ExtKeys::HOME, mod);
    }
    else if (ev->key() == Qt::Key_End)
    {
        pMode->AddKeyPress(ExtKeys::END, mod);
    }
    else if (ev->key() == Qt::Key_Right)
    {
        pMode->AddKeyPress(ExtKeys::RIGHT, mod);
    }
    else if (ev->key() == Qt::Key_Left)
    {
        pMode->AddKeyPress(ExtKeys::LEFT, mod);
    }
    else if (ev->key() == Qt::Key_Up)
    {
        pMode->AddKeyPress(ExtKeys::UP, mod);
    }
    else if (ev->key() == Qt::Key_Down)
    {
        pMode->AddKeyPress(ExtKeys::DOWN, mod);
    }
    else
    {

        mod |= (ev->modifiers() & Qt::ShiftModifier);
        QString input = ev->text();
        for (auto& i : input)
        {
            auto ch = i.toLatin1();
            if (ch != 0)
            {
                m_spEditor->GetCurrentMode()->AddKeyPress(i.toLatin1(), mod);
            }
        }
    }
    update();
}

}
