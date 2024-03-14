#pragma once

#include <QWidget>

class CWindowPasteWidget : public QWidget 
{
public:
	CWindowPasteWidget(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	bool paste(HWND child);

protected:
	virtual void paintEvent(QPaintEvent *event);
	virtual void moveEvent(QMoveEvent *event);
	virtual void closeEvent(QCloseEvent *event);

private:
	HWND mParent;
	HWND mChild;

	bool m_isPaste = false;
	/*子窗口粘贴进来时是否无边框.
		如果有边框，粘贴进来后需要去边框, 恢复原样时, 要恢复边框*/
	bool m_isWindowHint = false;
	bool m_isRecoveryChildWindowWhenQuit = true;
};