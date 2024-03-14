#include "CMultiProcManager.h"

#ifdef WIN32
#include <windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#endif
#include <QSet>
#include <QDir>
#include <QTimer>
#include <QFileInfo>
#include <QWidget>


typedef struct PorcessTopwnd
{
	HWND hwnd;
	DWORD dwProcessId;
	CHAR szTitle[256];
	QStringList lstTitle;//���иý�����ö�ٵ����д��ڵ�����, ��Ŀ�괰�ڴ���ʱ, ֻö�ٵ�Ŀ�괰��. ��Ŀ�괰�ڲ�����ʱ, ö�����д���
}PorcessTopWnd;

HWND GetHwndByPid(DWORD dwProcessId, const CHAR *szTitle, QStringList &lstTitle)
{
	PorcessTopWnd processTopwnd = { 0 };
	processTopwnd.dwProcessId = dwProcessId;
	lstrcpyA(processTopwnd.szTitle, szTitle);
	

	EnumWindows([](HWND hwnd, LPARAM lparam) {
		PorcessTopWnd *processTopWnd = (PorcessTopwnd *)lparam;

		DWORD dwProcessId = 0;
		GetWindowThreadProcessId(hwnd, &dwProcessId);
		if (dwProcessId == processTopWnd->dwProcessId)
		{
			if (lstrcmpA(processTopWnd->szTitle, "") == 0)
			{
				processTopWnd->hwnd = hwnd;
				return TRUE;
			}
			CHAR szTitle[128] = ("");
			::GetWindowTextA(hwnd, szTitle, 128);
			processTopWnd->lstTitle.append(szTitle);
			if (lstrcmpA(szTitle, processTopWnd->szTitle) == 0)
			{
				processTopWnd->hwnd = hwnd;
				return FALSE;
			}
		}
		return TRUE;
	}, (LPARAM)&processTopwnd);

	lstTitle = processTopwnd.lstTitle;
	return processTopwnd.hwnd;
}

sProcInfo::sProcInfo(const sProcInfo &procInfo)
{
	this->isRunning = procInfo.isRunning;
	this->strProcPath = procInfo.strProcPath;
	this->strDependPath = procInfo.strDependPath;
	this->lstUiName = procInfo.lstUiName;

	this->strProcName = procInfo.strProcName;
	this->strProcFileName = procInfo.strProcFileName;
	this->pProc = procInfo.pProc;
	this->iProcId = procInfo.iProcId;
	this->mapProcWnd = procInfo.mapProcWnd;
}

sProcInfo::sProcInfo(const QString &strProcPath, 
	const QStringList &lstUiName, 
	const QString &strDependPath /*= QString()*/,
	const QString &strProcName /*= QString()*/)
{
	if (!strProcPath.endsWith(".exe"))
		return;

	QFileInfo fileInfo(strProcPath);
	if (!fileInfo.exists() ||
		!fileInfo.isFile())
		return;
	
	this->strProcPath = QDir::toNativeSeparators(strProcPath);
	this->lstUiName = lstUiName;
	if (strProcName.isEmpty())
		this->strProcName = fileInfo.fileName();
	else
		this->strProcName = strProcName;
	this->strProcFileName = fileInfo.fileName();

	QFileInfo fileInfo2(strDependPath);
	if (strDependPath.isEmpty() ||
		!fileInfo2.exists() ||
		!fileInfo2.isDir())
	{
		this->strDependPath = QDir::toNativeSeparators(fileInfo.path());
	}
	else
	{
		this->strDependPath = strDependPath;
	}
}

CMultiProcManager* CMultiProcManager::m_pInstance = nullptr;

CMultiProcManager * CMultiProcManager::GetKernel()
{
	if (m_pInstance == nullptr)
	{
		m_pInstance = new CMultiProcManager;
	}
	return m_pInstance;
}

void CMultiProcManager::DestroyKernel()
{
	if (m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = nullptr;
	}
}

CMultiProcManager::CMultiProcManager(QObject *parent)
	: QObject(parent), m_pTimer(nullptr)
{
	qRegisterMetaType<sProcInfo>("sProcInfo");
	OnTimer();
}

CMultiProcManager::~CMultiProcManager()
{
	if (m_pTimer)
	{
		if (m_pTimer->isActive())
			m_pTimer->stop();

		delete m_pTimer;
		m_pTimer = nullptr;
	}

	m_mutexProcInfo.lock();
	for (auto it = m_mapProcInfo.begin(); it != m_mapProcInfo.end(); ++it)
	{
		if (it->pProc || it->isRunning)
		{
			if (it->bTerminateWhenClose)
			{
				/*�������Ը��ݲ�ͬ����Ŀ,ѡ���ӽ���֪ͨ�رա�ǿ�ƹر�*/
				if (it->pProc)
				{
					//it->pProc->close();
					//it->pProc->terminate()
					delete it->pProc;
					it->pProc = nullptr;
				}
				else
				{
					sProcInfo procInfo = *it;
					kill(procInfo);
				}
			}
		}
	}
	m_mutexProcInfo.unlock();
}

bool CMultiProcManager::kill(sProcInfo &pProc)
{
	if (isProcExist(pProc))
	{
		QProcess process;
		// ����Ҫִ�е�����Ͳ���
		QString command = "taskkill";
		QStringList arguments;
		arguments << "/F" << "/IM" << pProc.strProcName;
		// ��������
		process.start(command, arguments);
		return process.waitForFinished(-1);
	}
	return true;
}

void CMultiProcManager::start(sProcInfo &procInfo)
{
	QMutexLocker lock(&m_mutexProcInfo);
	if (!m_mapProcInfo.contains(procInfo.strProcPath))
		m_mapProcInfo.insert(procInfo.strProcPath, procInfo);
}

bool CMultiProcManager::remove(sProcInfo &procInfo)
{
	QMutexLocker lock(&m_mutexProcInfo);
	if (!m_mapProcInfo.contains(procInfo.strProcPath))
	{
		m_mapProcInfo.remove(procInfo.strProcPath);
		return true;
	}
	return false;
}

void CMultiProcManager::OnTimer()
{
	if (!m_pTimer)
	{
		m_pTimer = new QTimer;
		connect(m_pTimer, &QTimer::timeout, this, [&]() {
			if (!isAllProcExist() ||
				!isAllWndInManager())
			{
				m_pTimer->setInterval(m_iDenseInterval);
				m_mutexProcInfo.lock();
				for (auto it = m_mapProcInfo.begin(); it != m_mapProcInfo.end(); ++it)
				{
					if (!it->isRunning)//������δ����
					{
						if (it->pProc)
						{
							delete it->pProc;
							it->pProc = nullptr;
						}
						it->pProc = new QProcess;
						it->pProc->setProgram(it->strProcPath);
						it->pProc->setWorkingDirectory(it->strDependPath);
						it->mapProcWnd.clear();
						it->pProc->start();
						it->pProc->waitForStarted();
						it->iProcId = it->pProc->processId();
						it->isRunning = true;

						if (it->lstUiName.isEmpty())
							emit signalProcessStarted(*it);
					}
					else//����������
					{
						if (!it->lstUiName.isEmpty() &&
							it->mapProcWnd.isEmpty() ||
							it->mapProcWnd.count() < it->lstUiName.count())
						{
							bool ret(true);
							for (auto strUiName : it->lstUiName)
							{
								if (it->mapProcWnd.contains(strUiName) &&
									it->mapProcWnd[strUiName] &&
									QWindow::fromWinId(it->mapProcWnd[strUiName]) != nullptr)
									continue;

								qint64 processId = it->iProcId;
								QStringList lstTitle;
								HWND wnd = GetHwndByPid(processId, strUiName.toLocal8Bit().data(), lstTitle);
								if (!wnd)
								{
									emit signalProcessError(QString::fromLocal8Bit("���� %1(%2)��ȡ���� %3 ʧ��. �����б�: %4.")
										.arg(it->strProcPath)
										.arg(it->iProcId)
										.arg(strUiName)
										.arg(lstTitle.join(",")));
									if (m_bAutoRestartWhenGetWndFailed)
									{
										//�������ɱ���������, �ǲ������
										//����Ǳ�����������, ��Ȼ�޷��ҵ�����, �����Ҳ�����ҵ���, �������������κ�Ч��, ��Ҫ���崦��
										if (it->pProc == nullptr)
											kill(it.value());
									}

									ret = false;
									continue;
								}

								QWindow *childWindow = QWindow::fromWinId(WId(wnd));
								if (!childWindow)
								{
									emit signalProcessError(QString::fromLocal8Bit("���� %1(%2)��ȡ���� %3 ��Ч.")
										.arg(it->strProcPath)
										.arg(it->iProcId)
										.arg(strUiName));
									if (m_bAutoRestartWhenGetWndFailed)
									{
										//�������ɱ���������, �ǲ������
										//����Ǳ�����������, ��Ȼ�޷��ҵ�����, �����Ҳ�����ҵ���, �������������κ�Ч��, ��Ҫ���崦��
										if (it->pProc == nullptr)
											kill(it.value());
									}
									ret = false;
									continue;
								}
								it->mapProcWnd.insert(strUiName, WId(wnd));
							}
							if (ret)
								emit signalProcessStarted(*it);
						}
					}
				}
				m_mutexProcInfo.unlock();
			}
			else
			{
				m_pTimer->setInterval(m_iLooseInterval);
			}
		}, Qt::QueuedConnection);
		m_pTimer->setInterval(m_iDenseInterval);
		m_pTimer->start();
	}
}

bool CMultiProcManager::isProcInManager(const sProcInfo &procInfo)
{
	if (procInfo.isRunning &&
		//procInfo.pProc &&
		procInfo.iProcId /*&&
		procInfo.pProc->processId() == procInfo.iProcId*/)
	{
		QString strExecutablePath = getExecutablePathFromProcessId(procInfo.iProcId);
		if (procInfo.strProcPath == strExecutablePath)
			return true;
	}
	return false;
}

bool CMultiProcManager::isProcExist(sProcInfo &procInfo)
{
	//1.���������, ���Ѿ����ڼ����, �˶�����Ϣ֮��, ���ɷ��سɹ�
	if (isProcInManager(procInfo))
		return true;

	//2.���������δ���ڼ����, ��������н��̣��鿴Ŀ������Ƿ����
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);//������̿���
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return false;

	PROCESSENTRY32W ps;
	ZeroMemory(&ps, sizeof(PROCESSENTRY32W));
	ps.dwSize = sizeof(PROCESSENTRY32W);
	if (!Process32FirstW(hSnapshot, &ps))
	{
		CloseHandle(hSnapshot);
		return false;
	}

	do
	{
		QString strExeFile = QString::fromUtf16((ushort*)ps.szExeFile);
		if (strExeFile == procInfo.strProcName)
		{
			QString strExecutablePath = getExecutablePathFromProcessId(ps.th32ProcessID);
			if (procInfo.strProcPath == strExecutablePath)
			{
				if (ps.th32ProcessID != procInfo.iProcId)
					procInfo.iProcId = ps.th32ProcessID;
				return true;
			}
		}
	} while (Process32NextW(hSnapshot, &ps));
	CloseHandle(hSnapshot);
	return false;
}

bool CMultiProcManager::isAllProcInManager()
{
	QMutexLocker lock(&m_mutexProcInfo);
	for (auto it = m_mapProcInfo.cbegin(); it != m_mapProcInfo.cend(); ++it)
	{
		if (!isProcInManager(*it))
			return false;
	}
	return true;
}

bool CMultiProcManager::isAllProcExist()
{
	if (isAllProcInManager())
		return true;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);//������̿���
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return false;

	//��ʱ����
	m_mutexProcInfo.lock();
	QMap<QString, bool> mapProcExist;//
	QSet<QString> setProcName;//���еĽ�������
	for (auto it = m_mapProcInfo.cbegin(); it != m_mapProcInfo.cend(); ++it)
	{
		mapProcExist.insert(it.key(), false);
		setProcName.insert(it->strProcName);
	}
	m_mutexProcInfo.unlock();

	PROCESSENTRY32W ps;
	ZeroMemory(&ps, sizeof(PROCESSENTRY32W));
	ps.dwSize = sizeof(PROCESSENTRY32W);
	if (!Process32FirstW(hSnapshot, &ps))
	{
		CloseHandle(hSnapshot);
		return false;
	}

	do
	{
		QString strExeFile = QString::fromUtf16((ushort*)ps.szExeFile);
		if (setProcName.contains(strExeFile))
		{
			QString strExecutablePath = getExecutablePathFromProcessId(ps.th32ProcessID);
			m_mutexProcInfo.lock();
			if (m_mapProcInfo.contains(strExecutablePath))
			{
				mapProcExist[strExecutablePath] = true;
				sProcInfo &procInfo = m_mapProcInfo[strExecutablePath];
				procInfo.isRunning = true;
				if (ps.th32ProcessID != procInfo.iProcId)
				{
					procInfo.iProcId = ps.th32ProcessID;
				}

			}
			m_mutexProcInfo.unlock();
		}
	} while (Process32NextW(hSnapshot, &ps));
	CloseHandle(hSnapshot);

	bool ret(true);
	m_mutexProcInfo.lock();
	for (auto it = mapProcExist.cbegin(); it != mapProcExist.cend(); ++it)
	{
		if (!it.value())
		{
			m_mapProcInfo[it.key()].isRunning = false;
			ret = false;
		}
	}
	m_mutexProcInfo.unlock();
	return ret;
}

bool CMultiProcManager::isWndInManager(const sProcInfo &procInfo)
{
	if (procInfo.isRunning &&
		procInfo.iProcId)
	{
		if (procInfo.lstUiName.count() != procInfo.mapProcWnd.count())
			return false;

		for (auto itWnd = procInfo.mapProcWnd.cbegin(); 
			itWnd != procInfo.mapProcWnd.cend(); ++itWnd)
		{
			QWindow *childWindow = QWindow::fromWinId(itWnd.value());
			if (childWindow == nullptr)
				return false;
			//QWindow::fromWinIdֻ���ڴ��ھ����Ч�����ڵ�ǰӦ�ó���ʱ���ܳɹ�����QWindow����

			//�۲�һ�µ�����ճ��֮��, ��ȡ�Ľ���id���ӽ���id���Ǹ�����id
			DWORD dwProcessId = 0;
			GetWindowThreadProcessId((HWND)itWnd.value(), &dwProcessId);
			if (dwProcessId != procInfo.iProcId)
				dwProcessId = procInfo.iProcId;
		}
		return true;
	}

	return false;
}

bool CMultiProcManager::isAllWndInManager()
{
	bool ret(true);
	QMutexLocker lock(&m_mutexProcInfo);
	for (auto it = m_mapProcInfo.cbegin(); it != m_mapProcInfo.cend(); ++it)
	{
		sProcInfo procInfo = it.value();
		if (!isWndInManager(procInfo))
			return false;
	}
	return true;
}

QString CMultiProcManager::getExecutablePathFromProcessId(qint64 processId)
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, DWORD(processId));
	if (hProcess)
	{
		WCHAR buffer[MAX_PATH];
		DWORD bufferSize = sizeof(buffer) / sizeof(buffer[0]);
		if (GetModuleFileNameExW(hProcess, nullptr, buffer, bufferSize) > 0)
		{
			CloseHandle(hProcess);
			return QString::fromWCharArray(buffer);
		}
		CloseHandle(hProcess);
	}
	return QString();
}
