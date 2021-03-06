#include "StdAfx.h"

static char* __THIS_FILE__ = __FILE__;

#include "comm.h"

namespace Common{
	CComm::CComm()
        : _hComPort(NULL)
        , _nRead(0), _nWritten(0), _nQueued(0)
	{
		_begin_threads();


	}


	CComm::~CComm()
	{
		_end_threads();
	}

	bool CComm::open(int com_id)
	{
		if (is_opened()){
			SMART_ASSERT("com was opened!" && 0).Fatal();
			return false;
		}

		auto file = R"(\\.\COM)" + std::to_string(com_id);
		_hComPort = ::CreateFile(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (_hComPort == INVALID_HANDLE_VALUE){
			_hComPort = NULL;
			DWORD dwErr = ::GetLastError();
            system_error();
			if (dwErr == ERROR_FILE_NOT_FOUND){

			}
			return false;
		}


		return true;
	}

	bool CComm::close()
	{
		SMART_ENSURE(::CloseHandle(_hComPort), != 0).Fatal();
		_hComPort = NULL;
		_send_data.empty();

		return true;
	}

	void CComm::write(const void * data, unsigned int cb)
	{
		SMART_ASSERT(is_opened()).Stop();
		c_send_data_packet* psdp = _send_data.alloc((int)cb);
		::memcpy(&psdp->data[0], data, cb);

        switch (psdp->type)
        {
        case csdp_type::csdp_alloc:
        case csdp_type::csdp_local:
            update_counter(0, 0, psdp->cb);
            break;
        }

		_send_data.put(psdp);
	}

	void CComm::write(const std::string & data)
	{
		return write(data.c_str(), data.size());
	}

	bool CComm::begin_threads()
	{
		::ResetEvent(_thread_read.hEventToExit);
		::ResetEvent(_thread_write.hEventToExit);
		::ResetEvent(_thread_event.hEventToExit);
		
		::SetEvent(_thread_read.hEventToBegin);
		::SetEvent(_thread_write.hEventToBegin);
		::SetEvent(_thread_event.hEventToBegin);

		return true;
	}

	bool CComm::end_threads()
	{
		auto psdp = _send_data.alloc(0);
		psdp->type = csdp_type::csdp_exit;
		_send_data.put_front(psdp);

		::ResetEvent(_thread_read.hEventToBegin);
		::ResetEvent(_thread_write.hEventToBegin);
		::ResetEvent(_thread_event.hEventToBegin);

		::SetEvent(_thread_read.hEventToExit);
		::SetEvent(_thread_write.hEventToExit);
		::SetEvent(_thread_event.hEventToExit);
		
		// 在读写线程退出之前, 两个end均为激发状态
		// 必须等到两个线程均退出工作状态才能有其它操作
		debug_puts("等待 [读线程] 结束...");
		while (::WaitForSingleObject(_thread_read.hEventToExit, 0) == WAIT_OBJECT_0);
		debug_puts("等待 [写线程] 结束...");
		while (::WaitForSingleObject(_thread_write.hEventToExit, 0) == WAIT_OBJECT_0);
		debug_puts("等待 [事件线程] 结束...");
		while (::WaitForSingleObject(_thread_event.hEventToExit, 0) == WAIT_OBJECT_0);

		return true;
	}

	unsigned int __stdcall CComm::thread_helper(void* pv)
	{
		auto pctx = reinterpret_cast<thread_helper_context*>(pv);
		auto comm = pctx->that;
		auto which = pctx->which;

		delete pctx;

		switch (which)
		{
		case thread_helper_context::e_which::kEvent:
			return comm->thread_event();
		case thread_helper_context::e_which::kRead:
			return comm->thread_read();
		case thread_helper_context::e_which::kWrite:
			return comm->thread_write();
		default:
			SMART_ASSERT(0 && "unknown thread").Fatal();
			return 1;
		}
	}

	unsigned int CComm::thread_event()
	{
		BOOL bRet;
		DWORD dw,dw2;

	_wait_for_work:
		debug_puts("[事件线程] 就绪!");
		dw = ::WaitForSingleObject(_thread_event.hEventToBegin, INFINITE);
		SMART_ASSERT(dw == WAIT_OBJECT_0)(dw).Fatal();

		if (!is_opened()){
			debug_puts("[事件线程] 没有工作, 退出中...");
			::SetEvent(_thread_event.hEventToExit);
			return 0;
		}
		debug_puts("[事件线程] 开始工作...");

		c_overlapped o(true, false);

	_wait_again:
		DWORD dwEvent = 0;
		::ResetEvent(o.hEvent);
		dw = ::WaitCommEvent(_hComPort, &dwEvent, &o);
		if (dw != FALSE){
			_event_listener.call_listeners(dwEvent);
			goto _wait_again;
		}
		else{ // I/O is pending
			if (::GetLastError() == ERROR_IO_PENDING){
				HANDLE handles[2];
				handles[0] = _thread_event.hEventToExit;
				handles[1] = o.hEvent;

				switch (::WaitForMultipleObjects(_countof(handles), handles, FALSE, INFINITE))
				{
				case WAIT_FAILED:
                    system_error("[事件线程::Wait失败]");
					goto _restart;
					break;
				case WAIT_OBJECT_0 + 0:
					debug_puts("[事件线程] 收到退出事件!");
					goto _restart;
					break;
				case WAIT_OBJECT_0 + 1:
					bRet = ::GetOverlappedResult(_hComPort, &o, &dw2, FALSE);
					if (bRet == FALSE){
                        system_error("[事件线程::Wait失败]");
						goto _restart;
					}
					else{
						_event_listener.call_listeners(dwEvent); // uses dwEvent, not dw2
						goto _wait_again;
					}
					break;
				}
			}
			else{
                system_error("[事件线程]::GetLastError() != ERROR_IO_PENDING\n\n");
			}
		}

	_restart:
		if (!::CancelIo(_hComPort)){

		}

		::WaitForSingleObject(_thread_event.hEventToExit, INFINITE);
		::ResetEvent(_thread_event.hEventToExit);

		goto _wait_for_work;
	}

	unsigned int CComm::thread_write()
	{
		BOOL bRet;
		DWORD dw;

		c_event_event_listener listener;

	_wait_for_work:
		debug_puts("[写线程] 就绪");
		dw = ::WaitForSingleObject(_thread_write.hEventToBegin, INFINITE);
		SMART_ASSERT(dw == WAIT_OBJECT_0)(dw).Fatal();
		
		if (!is_opened()){
			debug_puts("[写线程] 没有工作, 退出中...");
			::SetEvent(_thread_write.hEventToExit);
			return 0;
		}
		debug_puts("[写线程] 开始工作...");

		c_overlapped overlap(false, false);
		
		_event_listener.add_listener(listener, EV_TXEMPTY);

	_get_packet:
		debug_puts("[写线程] 取数据包中...");
		c_send_data_packet* psdp = _send_data.get();
		if (psdp->type == csdp_type::csdp_alloc || psdp->type == csdp_type::csdp_local){
			debug_printll("[写线程] 取得一个发送数据包, 长度为 %d 字节", psdp->cb);

			DWORD	nWritten = 0;		// 写操作一次写入的长度
			int		nWrittenData;		// 当前循环总共写入长度

			for (nWrittenData = 0; nWrittenData < psdp->cb;){
				bRet = ::WriteFile(_hComPort, &psdp->data[0] + nWrittenData, psdp->cb - nWrittenData, NULL, &overlap);
				if (bRet != FALSE){ // I/O is completed
					bRet = ::GetOverlappedResult(_hComPort, &overlap, &nWritten, FALSE);
					if (bRet){
						debug_printll("[写线程] I/O completed immediately, bytes : %d", nWritten);
					}
					else{
                        system_error("[写线程] GetOverlappedResult失败(I/O completed)!\n");
						goto _restart;
					}
				}
				else{ // I/O is pending						
					if (::GetLastError() == ERROR_IO_PENDING){
						HANDLE handles[2];
						handles[0] = _thread_write.hEventToExit;
						handles[1] = listener.hEvent;

						switch (::WaitForMultipleObjects(_countof(handles), &handles[0], FALSE, INFINITE))
						{
						case WAIT_FAILED:
                            system_error("[写线程] Wait失败!\n");
							goto _restart;
							break;
						case WAIT_OBJECT_0 + 0: // now we exit
							debug_puts("[写线程] 收到退出事件!");
							goto _restart;
							break;
						case WAIT_OBJECT_0 + 1: // the I/O operation is now completed
							bRet = ::GetOverlappedResult(_hComPort, &overlap, &nWritten, FALSE);
							if (bRet){
								debug_printll("[写线程] 写入 %d 个字节!", nWritten);
							}
							else{
                                system_error("[写线程] GetOverlappedResult失败(I/O pending)!\n");
								goto _restart;
							}
							break;
						}
					}
					else{
                        system_error("[写线程] ::GetLastError() != ERROR_IO_PENDING");
						goto _restart;
					}
				}

				nWrittenData += nWritten;
                update_counter(0, nWritten, -(int)nWritten);
			}
			_send_data.release(psdp);
			goto _get_packet;
		}
		else if (psdp->type == csdp_type::csdp_exit){
			debug_puts("[写线程] 收到退出事件!");
			_send_data.release(psdp);
			goto _restart;
		}

	_restart:
		if (!::CancelIo(_hComPort)){

		}

		_event_listener.remove_listener(listener);
		listener.reset();

		// Do just like the thread_read do.
		::WaitForSingleObject(_thread_write.hEventToExit, INFINITE);
		::ResetEvent(_thread_write.hEventToExit);

		goto _wait_for_work;
	}

	unsigned int CComm::thread_read()
	{
#define MAX_BUFFER_VALIDITY 40 // ms
#define BUFFER_ACTUAL_SIZE (16 * 1024)
#define BUFFER_BLOB_SIZE (BUFFER_ACTUAL_SIZE - 4)
#define BUFFER_SINGLE_READ_SIZE (1024 * 4)
		bool buffer_flush = false;
		DWORD buffer_birth;
		unsigned char* buffer_pointer;
		unsigned char* buffer_boundary;
		unsigned int   buffer_usage;
		unsigned int   buffer_max_usage = 0;
		unsigned char* buffer_pool = NULL;
		buffer_pool = new unsigned char[BUFFER_ACTUAL_SIZE]; // make room for null-terminated string
		buffer_boundary = buffer_pool + (BUFFER_BLOB_SIZE - 1);

		c_event_event_listener listener;

	_wait_for_work:
		buffer_birth = GetTickCount();
		buffer_usage = 0;
		buffer_pointer = buffer_pool;
		debug_puts("[读线程] 就绪");
		DWORD dw = ::WaitForSingleObject(_thread_read.hEventToBegin, INFINITE);
		SMART_ASSERT(dw == WAIT_OBJECT_0)(dw).Fatal();

		if (!is_opened()){
			debug_puts("[读线程] 没有工作, 退出中...");
			delete[] buffer_pool;
			::SetEvent(_thread_read.hEventToExit);
			return 0;
		}
		debug_printll("[读线程] 开始工作...kReadBufSize:%d", BUFFER_ACTUAL_SIZE);

		c_overlapped overlap(false, false);

		_event_listener.add_listener(listener, EV_RXCHAR);


		HANDLE handles[2];
		handles[0] = _thread_read.hEventToExit; // exit
		handles[1] = listener.hEvent; // EV_RXCHAR

	_get_packet:
		switch (::WaitForMultipleObjects(_countof(handles), handles, FALSE, 100))
		{
		case WAIT_FAILED:
            system_error("[读线程] Wait失败!\n");
			goto _restart;
		case WAIT_OBJECT_0 + 0:
			debug_puts("[读线程] 收到退出事件!");
			debug_printll("buffer_max_usage:%d, buffer_usage:%d", buffer_max_usage, buffer_usage);
			goto _restart;
		case WAIT_OBJECT_0 + 1:
			break;
		case WAIT_TIMEOUT:
			if (buffer_usage) {
				debug_printll("WAIT_TIMEOUT flush:%d", buffer_usage);
				buffer_flush = true;
				break;
			}
			else {
				goto _get_packet;
			}
		}

		DWORD nBytesToRead;
		do {
			// WaitForData
			DWORD	comerr;
			COMSTAT	comsta;
			BOOL bRet = ::ClearCommError(_hComPort, &comerr, &comsta);
			if (CE_BREAK & comerr) {
				debug_puts("CE_BREAK");
			}

			if (CE_FRAME & comerr) {
				debug_puts("CE_FRAME");
			}

			if (CE_OVERRUN & comerr) {
				debug_puts("CE_OVERRUN");
				MessageBeep(MB_OK);
			}

			if (CE_RXOVER & comerr) {
				debug_puts("CE_RXOVER");
			}
			if (!bRet) {
				PrintLastError();
				system_error("ClearCommError()");
				goto _restart;
			}
			nBytesToRead = comsta.cbInQue;
		} while (0);

		debug_printlll("cbInQue:%d", nBytesToRead);
		if (nBytesToRead == 0 && !buffer_flush) {
			goto _get_packet;
		}

		if (nBytesToRead > (DWORD)(buffer_boundary - buffer_pointer)) {
			debug_printlll("shrink btr from %d to %d", nBytesToRead, buffer_boundary - buffer_pointer);
			nBytesToRead = buffer_boundary - buffer_pointer; // don't out-of range of buffer
		}

		for (DWORD nRead = 0; nBytesToRead > 0;) {
			//debug_printlll("btr:%d", nBytesToRead);
			BOOL bRet = ::ReadFile(_hComPort, buffer_pointer, nBytesToRead, &nRead, &overlap);
			//debug_printlll("read:%d, ReadFile:%d", nRead, bRet);
			if (bRet) { // the function succeeds
				bRet = ::GetOverlappedResult(_hComPort, &overlap, &nRead, FALSE);
				if (bRet) {
					//debug_printlll("read:%d, bRet==TRUE, btr:%d", nRead, nBytesToRead);
				}
				else {
					system_error("[读线程] GetOverlappedResult失败!\n");
					goto _restart;
				}
			}
			else { // can't read specified amount
				if (::GetLastError() == ERROR_IO_PENDING) {
					HANDLE handles[2];
					handles[0] = _thread_read.hEventToExit;
					handles[1] = overlap.hEvent;

					switch (::WaitForMultipleObjects(_countof(handles), &handles[0], FALSE, INFINITE)) {
					case WAIT_FAILED:
						debug_puts("[读线程] 等待失败!");
						goto _restart;
					case WAIT_OBJECT_0 + 0:
						debug_puts("[读线程] 收到退出事件!");
						debug_printll("buffer_max_usage:%d", buffer_max_usage);
						goto _restart;
					case WAIT_OBJECT_0 + 1:
						bRet = ::GetOverlappedResult(_hComPort, &overlap, &nRead, FALSE);
						if (bRet) {
							debug_printlll("read:%d", nRead);
						}
						else {
							system_error("[读线程] GetOverlappedResult失败!\n");
							goto _restart;
						}
						break;
					}
				}
				else {
					system_error("[读线程] ::GetLastError() != ERROR_IO_PENDING"); // MessageBox
					goto _restart;
				}
			}

			if (nRead > 0) {
				nBytesToRead -= nRead;
				buffer_pointer += nRead;
				buffer_usage += nRead;
			}
			else {
				debug_printlll("error read:%d, btr:%d", nRead, nBytesToRead);
				// WaitForData
				DWORD	comerr;
				COMSTAT	comsta;
				BOOL bRet = ::ClearCommError(_hComPort, &comerr, &comsta);
				if (CE_BREAK & comerr) {
					debug_puts("CE_BREAK");
				}

				if (CE_FRAME & comerr) {
					debug_puts("CE_FRAME");
				}

				if (CE_OVERRUN & comerr) {
					debug_puts("CE_OVERRUN");
					MessageBeep(MB_OK);
				}

				if (CE_RXOVER & comerr) {
					debug_puts("CE_RXOVER");
				}
				if (!bRet) {
					PrintLastError();
					system_error("ClearCommError()");
				}
				break;
			}
		}

		if (buffer_pointer == buffer_pool) {
			debug_printlll("buffer_pointer == buffer_pool");
			MessageBeep(MB_OK);
			goto _get_packet;
		}

		DWORD tc_current = GetTickCount();
		debug_printll("tc_current - buffer_birth = %d - %d = %d", tc_current, buffer_birth, tc_current - buffer_birth);
		if (!buffer_flush && MAX_BUFFER_VALIDITY > tc_current - buffer_birth) {
			debug_printll("buffer recv:%d, buffer_usage:%d", buffer_pointer - buffer_pool, buffer_usage);
		}
		else {
			buffer_max_usage = max(buffer_max_usage, buffer_usage);
			*buffer_pointer = '\0'; // null-terminated string
			//debug_printlll("update_counter:%d", buffer_pointer - buffer_pool);
			update_counter(buffer_usage, 0, 0);
			auto cmd = new Command_ReceiveData;
			cmd->data.assign((const char *)buffer_pool, buffer_usage);
			_commands.push_back(cmd);
			if (buffer_flush) {
				buffer_flush = false;
				debug_printlll("[flush]ReceiveData:%d", buffer_usage);
			}
			else {
				debug_printlll("ReceiveData:%d", buffer_usage);
			}
			buffer_pointer = buffer_pool;
			buffer_usage = 0;
			buffer_birth = GetTickCount();
		}
		goto _get_packet;

	_restart:
		if (!::CancelIo(_hComPort)){

		}

		_event_listener.remove_listener(listener);
		listener.reset();

		::WaitForSingleObject(_thread_read.hEventToExit, INFINITE);
		::ResetEvent(_thread_read.hEventToExit);

		goto _wait_for_work;
	}

	//////////////////////////////////////////////////////////////////////////

	bool CComm::setting_comm(s_setting_comm* pssc)
	{
		SMART_ASSERT(is_opened()).Fatal();

		DCB _dcb;

		if (!::GetCommState(_hComPort, &_dcb)){
            system_error("GetCommState()错误");
			return false;
		}

		_dcb.fBinary = TRUE;
		_dcb.BaudRate = pssc->baud_rate;
		_dcb.fParity = pssc->parity == NOPARITY ? FALSE : TRUE;
		_dcb.Parity = pssc->parity;
		_dcb.ByteSize = pssc->databit;
		_dcb.StopBits = pssc->stopbit;

		if (!::SetCommState(_hComPort, &_dcb)){
            system_error("SetCommState()错误");
			return false;
		}

		if (!::SetCommMask(_hComPort, 
			EV_RXCHAR|EV_RXFLAG|EV_TXEMPTY
			| EV_CTS | EV_DSR | EV_RLSD
			| EV_BREAK | EV_ERR
			| EV_RING
			| EV_PERR | EV_RX80FULL))
		{
            system_error("SetCommMask()错误");
			return false;
		}

		COMMTIMEOUTS _timeouts;
		_timeouts.ReadIntervalTimeout = MAXDWORD;
		_timeouts.ReadTotalTimeoutMultiplier = 0;
		_timeouts.ReadTotalTimeoutConstant = 100;
		_timeouts.WriteTotalTimeoutMultiplier = 0;
		_timeouts.WriteTotalTimeoutConstant = 0;
		if (!::SetCommTimeouts(_hComPort, &_timeouts)){
			system_error("SetCommTimeouts()错误");
			return false;
		}

		PurgeComm(_hComPort, PURGE_TXCLEAR | PURGE_TXABORT);
		PurgeComm(_hComPort, PURGE_RXCLEAR | PURGE_RXABORT);

		return true;
	}

	bool CComm::_begin_threads()
	{
		thread_helper_context* pctx = nullptr;

		// 开启读线程
		_thread_read.hEventToBegin = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		_thread_read.hEventToExit = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

		pctx = new thread_helper_context;
		pctx->that = this;
		pctx->which = thread_helper_context::e_which::kRead;
		_thread_read.hThread = (HANDLE)::_beginthreadex(nullptr, 0, thread_helper, pctx, 0, nullptr);

		if (!(_thread_read.hEventToBegin && _thread_read.hEventToExit && _thread_read.hThread)) {
			system_error("严重错误。");
			return false;
		}

		// 开启写线程
		_thread_write.hEventToBegin = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		_thread_write.hEventToExit = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

		pctx = new thread_helper_context;
		pctx->that = this;
		pctx->which = thread_helper_context::e_which::kWrite;
		_thread_write.hThread = (HANDLE)::_beginthreadex(nullptr, 0, thread_helper, pctx, 0, nullptr);

		if (!(_thread_write.hEventToBegin && _thread_write.hEventToExit && _thread_write.hThread)) {
			system_error("严重错误。");
			return false;
		}

		// 开启事件线程
		_thread_event.hEventToBegin = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		_thread_event.hEventToExit = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

		pctx = new thread_helper_context;
		pctx->that = this;
		pctx->which = thread_helper_context::e_which::kEvent;
		_thread_event.hThread = (HANDLE)::_beginthreadex(nullptr, 0, thread_helper, pctx, 0, nullptr);

		if (!(_thread_event.hEventToBegin && _thread_event.hEventToExit && _thread_event.hThread)) {
			system_error("严重错误。");
			return false;
		}

		return true;
	}

	bool CComm::_end_threads()
	{
		SMART_ASSERT(is_opened() == false).Fatal();

		// 由线程在退出之前设置并让当前线程等待他们的结束
		::ResetEvent(_thread_read.hEventToExit);
		::ResetEvent(_thread_write.hEventToExit);
		::ResetEvent(_thread_event.hEventToExit);

		// 此时串口是关闭的, 收到此事件即准备退出线程
		::SetEvent(_thread_read.hEventToBegin);
		::SetEvent(_thread_write.hEventToBegin);
		::SetEvent(_thread_event.hEventToBegin);

		// 等待线程完全退出
		::WaitForSingleObject(_thread_read.hEventToExit, INFINITE);
		::WaitForSingleObject(_thread_write.hEventToExit, INFINITE);
		::WaitForSingleObject(_thread_event.hEventToExit, INFINITE);

		::CloseHandle(_thread_read.hEventToBegin);
		::CloseHandle(_thread_read.hEventToExit);
		::CloseHandle(_thread_write.hEventToBegin);
		::CloseHandle(_thread_write.hEventToExit); 
		::CloseHandle(_thread_event.hEventToBegin);
		::CloseHandle(_thread_event.hEventToExit);

		::CloseHandle(_thread_read.hThread);
		::CloseHandle(_thread_write.hThread);
		::CloseHandle(_thread_event.hThread);

		return false;
	}

    void CComm::update_counter(int nRead, int nWritten, int nQueued) {
        if(nRead)       InterlockedExchangeAdd(&_nRead, nRead);
        if(nWritten)    InterlockedExchangeAdd(&_nWritten, nWritten);
        if(nQueued)     InterlockedExchangeAdd(&_nQueued, nQueued);

        if(nRead || nWritten || nQueued) {
            auto cmd = new Command_UpdateCounter;
            cmd->nRead = nRead;
            cmd->nWritten = nWritten;
            cmd->nQueued = nQueued;
            _commands.push_back(cmd);
        }
    }

    void CComm::reset_counter(bool r, bool w, bool q) {
        if(r) InterlockedExchange(&_nRead, 0);
        if(w) InterlockedExchange(&_nWritten, 0);
        if(q) InterlockedExchange(&_nQueued, 0);

        auto cmd = new Command_UpdateCounter;
        cmd->nRead = _nRead;
        cmd->nWritten = _nWritten;
        cmd->nQueued = _nQueued;
        _commands.push_back(cmd);
    }

    void CComm::system_error(const std::string& prefix) {
        const char* msg = nullptr;
        std::string s(prefix);
        int code = GetLastError();

        if (s.empty()) s = "错误";

        if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPSTR)&msg, 1, NULL))
        {
            s += "：";
            s += msg;
            LocalFree((HLOCAL)msg);
        }

        auto cmd = new Command_ErrorMessage;
        cmd->code = code;
        cmd->what = std::move(s);

        _commands.push_back(cmd);
    }

    void CComm::get_counter(int* pRead, int* pWritten, int* pQueued) {
        *pRead = _nRead;
        *pWritten = _nWritten;
        *pQueued = _nQueued;
    }

	//////////////////////////////////////////////////////////////////////////
	c_data_packet_manager::c_data_packet_manager()
		: _hEvent(0)
	{
		_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		SMART_ASSERT(_hEvent != NULL).Fatal();
		
		list_init(&_list);
		for (int i = 0; i < sizeof(_data) / sizeof(_data[0]); i++)
			_data[i].used = false;
	}

	c_data_packet_manager::~c_data_packet_manager()
	{
		SMART_ASSERT(list_is_empty(&_list)).Fatal();
		::CloseHandle(_hEvent);
	}

	c_send_data_packet* c_data_packet_manager::alloc(int size)
	{
		SMART_ASSERT(size >= 0)(size).Fatal();
		_lock.lock();

		c_send_data_packet* psdp = NULL;

		if (size <= csdp_def_size){
			for (int i = 0; i < sizeof(_data) / sizeof(_data[0]); i++){
				if (_data[i].used == false){
					psdp = (c_send_data_packet*)&_data[i];
					break;
				}
			}
			if (psdp != NULL){
				psdp->used = true;
				psdp->type = csdp_type::csdp_local;
				psdp->cb = size;
				goto _exit;
			}
			// no left
		}

		psdp = (c_send_data_packet*)new char[sizeof(c_send_data_packet) + size];
		psdp->type = csdp_type::csdp_alloc;
		psdp->used = true;
		psdp->cb = size;
		goto _exit;

	_exit:
		_lock.unlock();
		return psdp;
	}

	void c_data_packet_manager::release(c_send_data_packet* psdp)
	{
		SMART_ASSERT(psdp != NULL).Fatal();

		switch (psdp->type)
		{
		case csdp_type::csdp_alloc:
			delete[] psdp;
			break;
		case csdp_type::csdp_local:
		case csdp_type::csdp_exit:
			_lock.lock();
			psdp->used = false;
			_lock.unlock();
			break;
		default:
			SMART_ASSERT(0).Fatal();
		}
	}

	void c_data_packet_manager::put(c_send_data_packet* psdp)
	{
		_lock.lock();
		list_insert_tail(&_list, &psdp->_list_entry);
		_lock.unlock();
		::SetEvent(_hEvent); // singal get() proc
	}

	c_send_data_packet* c_data_packet_manager::get()
	{
		c_send_data_packet* psdp = NULL;

		for (;;){ // 无限等待, 直到收到一个数据包
			_lock.lock();
			list_s* pls = list_remove_head(&_list);
			_lock.unlock();

			if (pls != NULL){
				psdp = list_data(pls, c_send_data_packet, _list_entry);
				return psdp;
			}
			else{
				::WaitForSingleObject(_hEvent, INFINITE);
			}
		}
	}

	void c_data_packet_manager::put_front(c_send_data_packet* psdp)
	{
		_lock.lock();
		list_insert_head(&_list, &psdp->_list_entry);
		_lock.unlock();
		::SetEvent(_hEvent);
	}

	void c_data_packet_manager::empty()
	{
		while (!list_is_empty(&_list)){
			list_s* p = list_remove_head(&_list);
			c_send_data_packet* psdp = list_data(p, c_send_data_packet, _list_entry);
			release(psdp);
		}
	}

	c_send_data_packet* c_data_packet_manager::query_head()
	{
		c_send_data_packet* psdp = NULL;
		_lock.lock();
		if (list_is_empty(&_list)){
			psdp = NULL;
		}
		else{
			psdp = list_data(_list.next, c_send_data_packet, _list_entry);
		}
		_lock.unlock();
		return psdp;
	}

}
