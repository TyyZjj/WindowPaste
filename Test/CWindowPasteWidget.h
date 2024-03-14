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
	/*�Ӵ���ճ������ʱ�Ƿ��ޱ߿�.
		����б߿�ճ����������Ҫȥ�߿�, �ָ�ԭ��ʱ, Ҫ�ָ��߿�*/
	bool m_isWindowHint = false;
	bool m_isRecoveryChildWindowWhenQuit = true;
};