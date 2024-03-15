#include "Test.h"
#include "CMultiProcManager.h"


Test::Test(QWidget *parent)
    : CWindowPasteWidget(parent)
{
    ui.setupUi(this);

	QStringList lstWndName;
	lstWndName.append(QString::fromLocal8Bit("Window Name"));
	sProcInfo procInfo("E:/NewGit/xxxx.exe", lstWndName);
	CMultiProcManager::GetKernel()->start(procInfo);

	connect(CMultiProcManager::GetKernel(), &CMultiProcManager::signalProcessStarted, this, 
		[&](sProcInfo procInfo) {
		if (!procInfo.mapProcWnd.isEmpty())
		{
			WId wid = procInfo.mapProcWnd.first();
			paste((HWND)wid);
		}
	}, Qt::QueuedConnection);
}
