#include "CWindowPasteWidget.h"
#include <windows.h>

CWindowPasteWidget::CWindowPasteWidget(QWidget* parent/* = nullptr*/, Qt::WindowFlags f /*= Qt::WindowFlags()*/)
	:QWidget(parent, f),mParent(nullptr), mChild(nullptr){}

bool CWindowPasteWidget::paste(HWND child)
{
	this->mParent = (HWND)winId();
	this->mChild = child;
	int iCount = 0;
	do //父进程刚启动时, 有可能父窗口还未创建成功
	{
		HWND wnd = SetParent(mChild, mParent);
		if (wnd)
		{
			RECT rect;
			GetWindowRect(mParent, &rect);
			BOOL ret = MoveWindow(mChild,
				0, 0,
				rect.right - rect.left, rect.bottom - rect.top, TRUE);

			LONG_PTR style = GetWindowLongPtr(mChild, GWL_STYLE);
			if ((style & (WS_BORDER | WS_THICKFRAME)))
			{
				style &= ~(WS_BORDER | WS_THICKFRAME);
				m_isWindowHint = false;
				SetWindowLongPtr(mChild, GWL_STYLE, style);
			}
			else
			{
				m_isWindowHint = true;
			}
			m_isPaste = true;
			return true;
		}
		iCount++;
	} while (iCount < 2);
	return false;

}

void CWindowPasteWidget::paintEvent(QPaintEvent *event)
{
	if (m_isPaste && mParent && mChild)
	{
		RECT rect;
		GetWindowRect(mParent, &rect);
		MoveWindow(mChild,
			0, 0,
			rect.right - rect.left, rect.bottom - rect.top, TRUE);
		m_isPaste = false;
	}
	return QWidget::paintEvent(event);
}

void CWindowPasteWidget::moveEvent(QMoveEvent *event)
{
	if (mParent && mChild)
	{
		RECT rect;
		GetWindowRect(mParent, &rect);
		MoveWindow(mChild,
			0, 0,
			rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}
	return QWidget::moveEvent(event);
}

void CWindowPasteWidget::closeEvent(QCloseEvent *event)
{
	if (m_isRecoveryChildWindowWhenQuit)
	{
		if (mParent && mChild)
		{
			RECT rect;
			GetWindowRect(mChild, &rect);
			HWND wnd = SetParent(mChild, nullptr);
			if (!m_isWindowHint)
			{
				LONG_PTR style = GetWindowLongPtr(mChild, GWL_STYLE);
				style |= (WS_BORDER | WS_THICKFRAME);
				SetWindowLongPtr(mChild, GWL_STYLE, style);
				SetWindowPos(mChild, nullptr,
					rect.left, rect.top,
					rect.right - rect.left, rect.bottom - rect.top,
					SWP_NOSIZE | SWP_SHOWWINDOW);
			}
		}
	}
}

