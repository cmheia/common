#include "StdAfx.h"
#include "WindowManager.h"

namespace Common{
	c_ptr_array<CWindowManager> CWindowManager::m_aWndMgrs;

	CWindowManager::CWindowManager()
		: m_hWnd(0)
		, m_pMsgFilter(0)
		, m_pAcceTrans(0)
		, m_pIdleHandler(0)
	{

	}

	CWindowManager::~CWindowManager()
	{

	}

	bool CWindowManager::FilterMessage(MSG* pmsg)
	{
		return m_pMsgFilter && m_pMsgFilter->FilterMessage(
			pmsg->hwnd, pmsg->message, pmsg->wParam, pmsg->lParam);
	}

	bool CWindowManager::AddWindowManager(CWindowManager* pwm)
	{
		return m_aWndMgrs.add(pwm);
	}

	bool CWindowManager::RemoveWindowManager(CWindowManager* pwm)
	{
		return m_aWndMgrs.remove(pwm);
	}

	bool CWindowManager::TranslateAccelerator( MSG* pmsg )
	{
		return m_pAcceTrans && m_pAcceTrans->TranslateAccelerator(pmsg);
	}

	bool CWindowManager::OnIdle(int count)
	{
		return m_pIdleHandler && m_pIdleHandler->OnIdle(count);
	}

	void CWindowManager::MessageLoop()
	{
		MSG msg;

		for (;;) {
			if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					break;
				}

				if (!TranslateMessage(&msg)) {
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
			}
			else {
				int n = 0;
				for (;; n++) {
					bool handled = false;
					for (int i = 0; i < m_aWndMgrs.size(); i++) {
						CWindowManager* pWM = m_aWndMgrs.getat(i);
						if (pWM->OnIdle(n)) {
							handled = true;
						}
					}
					if (!handled) {
						break;
					}
				}

				if (n == 0) {
					if (!::GetMessage(&msg, NULL, 0, 0)) {
						break;
					}

					if (!TranslateMessage(&msg)) {
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
				}
			}
		}
	}

	bool CWindowManager::TranslateMessage( MSG* pmsg )
	{
		bool bChild = !!(GetWindowStyle(pmsg->hwnd) & WS_CHILD);
		if(bChild){
			HWND hParent = pmsg->hwnd;
			while (hParent && ::GetWindowLongPtr(hParent, GWL_STYLE)&WS_CHILD){
				hParent = ::GetParent(hParent);
			}

			if (hParent != NULL){
				for (int i = 0; i < m_aWndMgrs.size(); i++){
					CWindowManager* pWM = m_aWndMgrs.getat(i);
					if (pWM->hWnd() == hParent){
						if (pWM->TranslateAccelerator(pmsg))
							return true;
						if (pWM->FilterMessage(pmsg))
							return true;
						return false;
					}
				}
			}
		}
		else{
			for(int i=0; i<m_aWndMgrs.size(); i++){
				CWindowManager* pWM = m_aWndMgrs.getat(i);
				if(pmsg->hwnd == pWM->hWnd()){
					if(pWM->TranslateAccelerator(pmsg))
						return true;
					if(pWM->FilterMessage(pmsg))
						return true;

					return false;
				}
			}
		}
		return false;
	}

	void CWindowManager::Init( HWND hWnd , IMessageFilter* flt)
	{
		//TODO
		//assert((GetWindowLongPtr(hWnd, GWL_STYLE)&WS_CHILD) == 0);
		m_hWnd = hWnd;
		AddWindowManager(this);
		MessageFilter() = flt;
	}

	void CWindowManager::DeInit()
	{
		RemoveWindowManager(this);
	}

}
