#include "stdafx.h"
#include "../debug.h"

#define DEBUG_UPDATE_TIMER_PERIOD 0

namespace Common{
namespace Window{

	//////////////////////////////////////////////////////////////////////////
	bool c_edit::back_delete_char( int n )
	{
		return false;
	}

	bool c_edit::_append_text( const char* str )
	{
		int len = ::GetWindowTextLength(m_hWnd);
		Edit_SetSel(m_hWnd, len, len);
		Edit_ReplaceSel(m_hWnd, str);
		debug_printll("max buffer:%u", _sz_buffer_max_usage);
		return true;
	}

	bool c_edit::append_text( const char* str, size_t len )
	{
		size_t length = len;

		if (0 == length) {
			length = strlen(str);
			if (0 == length) {
				return false;
			}
		}
		debug_printll("%d", length);

		if (_sz_buffer_size < length + _sz_buffer_usage) {
			if (0 == _sz_buffer_usage) {
				debug_printll("huge blob skip buffer[%d]", length);
				MessageBeep(MB_OK);
				_append_text(str);
			}
			else {
#ifdef _DEBUG
				if (_sz_buffer_usage > _sz_buffer_max_usage) {
					_sz_buffer_max_usage = _sz_buffer_usage;
				}
#endif // _DEBUG
				debug_printll("flush buffer:%u", _sz_buffer_usage);
				_buffer[_sz_buffer_usage] = '\0'; // null-terminated string
				_append_text(_buffer);
				_sz_buffer_usage = 0;
			}
		}
		memcpy(&(_buffer[_sz_buffer_usage]), str, length);
		_sz_buffer_usage += length;
		_buffer[_sz_buffer_usage] = '\0';
		return true;
	}

	void c_edit::update_timer_period(void)
	{
#if DEBUG_UPDATE_TIMER_PERIOD
		static DWORD ts = 0;
#endif // DEBUG_UPDATE_TIMER_PERIOD
		//(void)stop_replace_timer();
		if (0 < _sz_buffer_usage) {
#ifdef _DEBUG
			if (_sz_buffer_usage > _sz_buffer_max_usage) {
				_sz_buffer_max_usage = _sz_buffer_usage;
			}
#endif // _DEBUG
			debug_printll("periodical flush buffer:%u", _sz_buffer_usage);
#if DEBUG_UPDATE_TIMER_PERIOD
			DWORD nts = GetTickCount();
			printf(_DEBUG_STRING_FILE_LINE_FUNC " %d\t\n", nts - ts);
			ts = nts;
#endif // DEBUG_UPDATE_TIMER_PERIOD
			_buffer[_sz_buffer_usage] = '\0'; // null-terminated string
			_append_text(_buffer);
			_sz_buffer_usage = 0;
		}
		//start_replace_timer();
	}

	int c_edit::get_replace_timer(void)
	{
		return _replace_timer.get_period();
	}

	void c_edit::set_replace_timer(int ms)
	{
		_replace_timer.set_period(ms);
	}

	void c_edit::start_replace_timer(void)
	{
		stop_replace_timer();
		_replace_timer.start();
	}

	int c_edit::stop_replace_timer(void)
	{
		if (_replace_timer.is_running()) {
			_replace_timer.stop();
		}
		return _replace_timer.get_period();
	}

	void c_edit::limit_text( int sz )
	{
		SendMessage(EM_LIMITTEXT, sz == -1 ? 0 : sz);
	}

	void c_edit::set_text(const char* str)
	{
		::SetWindowText(*this, str);
	}

	HMENU c_edit::_load_default_menu()
	{
		return nullptr;
	}

	LRESULT c_edit::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled)
	{
		if (uMsg == WM_CONTEXTMENU) { 
			if (!_bUseDefMenu){
				return ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
			}
			else{

			}
		}
		return __super::HandleMessage(uMsg, wParam, lParam, bHandled);
	}

	bool c_edit::is_read_only()
	{
		return !!(::GetWindowLongPtr(*this, GWL_STYLE) & ES_READONLY);
	}

	//////////////////////////////////////////////////////////////////////////
	bool c_rich_edit::back_delete_char( int n )
	{
		int cch;
		GETTEXTLENGTHEX gtl;

		if(n <= 0)
			return false;

		gtl.flags = GTL_DEFAULT;
		gtl.codepage = CP_ACP;
		cch = SendMessage(EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
		if(cch > 0){
			if(n >= cch){
				SetWindowText(m_hWnd, "");
				return true;
			}
			else{
				CHARRANGE rng;
				rng.cpMax = cch;
				rng.cpMin = cch - n;
				SendMessage(EM_EXSETSEL, 0, (LPARAM)&rng);
				SendMessage(EM_REPLACESEL, FALSE, (LPARAM)"");
			}
		}
		return true;
	}
#if 0
	bool c_rich_edit::append_text( const char* str )
	{
		GETTEXTLENGTHEX gtl;
		CHARRANGE rng;
		int cch;

		gtl.flags = GTL_DEFAULT;
		gtl.codepage = CP_ACP;
		cch = SendMessage(EM_GETTEXTLENGTHEX, (WPARAM)&gtl);

		rng.cpMax = cch;
		rng.cpMin = cch;
		SendMessage(EM_EXSETSEL, 0, (LPARAM)&rng);
		Edit_ReplaceSel(m_hWnd,str);

		// Richedit bug: EM_SCROLLCARET will not work if a richedit gets no focus
		// http://stackoverflow.com/questions/9757134/scrolling-richedit-without-it-having-focus
		SendMessage(WM_VSCROLL, SB_BOTTOM);

		return true;
	}
#endif
	bool c_rich_edit::_append_text(const char* str, UINT codepage)
	{
		SETTEXTEX st = {ST_NEWCHARS | ST_SELECTION, codepage};
		GETTEXTLENGTHEX gtl;
		CHARRANGE rng;
		int cch;

		gtl.flags = GTL_DEFAULT;
		gtl.codepage = codepage;
		cch = SendMessage(EM_GETTEXTLENGTHEX, (WPARAM)&gtl);

		rng.cpMax = cch;
		rng.cpMin = cch;
		//debug_printll("append_text %d", cch);
		SendMessage(EM_EXSETSEL, 0, (LPARAM)&rng);
		//((void)::SendMessage((m_hWnd), EM_REPLACESEL, 0L, (LPARAM)(LPCTSTR)(str)));
		((void)::SendMessage((m_hWnd), EM_SETTEXTEX, (WPARAM)&st, (LPARAM)(LPCTSTR)(str)));

		// Richedit bug: EM_SCROLLCARET will not work if a richedit gets no focus
		// http://stackoverflow.com/questions/9757134/scrolling-richedit-without-it-having-focus
		SendMessage(WM_VSCROLL, SB_BOTTOM);

		debug_printll("max buffer:%u", _sz_buffer_max_usage);
		return true;
	}

	bool c_rich_edit::append_text(const char* str, UINT codepage)
	{
		// FIXME: check codepage
		//if (_codepage == codepage) { // can't change codepage on the fly, flush buffer needed
			size_t len = strlen(str);
			debug_printll("%d, %d", len, codepage);
			if (_sz_buffer_size < len + _sz_buffer_usage) {
				if (0 == _sz_buffer_usage) {
					debug_printll("huge blob skip buffer[%d]", len);
					MessageBeep(MB_OK);
					_append_text(str, _codepage);
				}
				else {
#ifdef _DEBUG
					if (_sz_buffer_usage > _sz_buffer_max_usage) {
						_sz_buffer_max_usage = _sz_buffer_usage;
					}
#endif // _DEBUG
					debug_printll("flush buffer:%u", _sz_buffer_usage);
					_buffer[_sz_buffer_usage] = '\0'; // null-terminated string
					_append_text(_buffer, _codepage);
					_sz_buffer_usage = 0;
					// 启动超时计数, 结束时调用 _pre_proc
				}
			}
			strcpy(&(_buffer[_sz_buffer_usage]), str);
			_sz_buffer_usage += len;
			return true;
		//}
		//else {
		//	debug_printll("can't change codepage from %d to %d on the fly", _codepage, codepage);
		//	return false;
		//}
	}

	bool c_rich_edit::append_byte(const char* str, size_t len, UINT codepage)
	{
		// FIXME: check codepage
		//if (_codepage == codepage) { // can't change codepage on the fly, flush buffer needed
			debug_printll("%d, %d", len, codepage);
			memcpy(&(_buffer[_sz_buffer_usage]), str, len);
			_sz_buffer_usage += len;
			_buffer[_sz_buffer_usage] = '\0';
			return true;
		//}
		//else {
		//	debug_printll("can't change codepage from %d to %d on the fly", _codepage, codepage);
		//	return false;
		//}
	}

	UINT c_rich_edit::get_cur_codepage(void)
	{
		return _codepage;
	}

	void c_rich_edit::set_cur_codepage(UINT codepage)
	{
		debug_printll("change codepage from %d to %d", _codepage, codepage);
		_codepage = codepage;
	}

	bool c_rich_edit::apply_linux_attributes(char* attrs)
	{
		const char* p = attrs;
		const char* end; 

		if (!attrs || !*attrs)
			return false;
		
		end = attrs + strlen(attrs) - 1;

		assert(*p++ == '\033');
		assert(*p++ == '[');

		switch (*end)
		{
		case 'm': // \033[a1;a1;...m
		{
			for (;*p != 'm';){
				if (*p == ';'){
					p++;
				}
				else if (*p >= '0' && *p <= '9'){
					int a = 0;
					p += read_integer(p, &a);
					apply_linux_attribute_m(a);
				}
				else{
					SMART_ASSERT(0)(*p).Fatal();
				}
			}
			break;
		}

		}


		return true;
	}

	bool c_rich_edit::apply_linux_attribute_m(int attr)
	{
		CHARFORMAT2 cf;
		static struct{
			int k;
			COLORREF v;
		}def_colors[] = 
		{
			{30, RGB(0,    0,  0)},
			{31, RGB(255,  0,  0)},
			{32, RGB(0,  255,  0)},
			{33, RGB(255,255,  0)},
			{34, RGB(0,    0,255)},
			{35, RGB(255,  0,255)},
			{36, RGB(0,  255,255)},
			{37, RGB(255,255,255)},
			{-1, RGB(0,0,0)      },
		};

		cf.cbSize = sizeof(cf);

		if(attr>=30 && attr<=37){
			cf.dwMask = CFM_COLOR;
			cf.dwEffects = 0;
			assert(_deffg>=30 && _deffg<=37);
			cf.crTextColor = def_colors[attr-30].v;
			SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		}
		else if(attr>=40 && attr<=47){
			cf.dwMask = CFM_BACKCOLOR;
			cf.dwEffects = 0;
			assert(_defbg>=40 && _defbg<=47);
			cf.crBackColor = def_colors[attr-40].v;
			SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		}
		else if(attr == 0){
			cf.dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD;
			cf.dwEffects = 0;
			assert( (_deffg>=30 && _deffg<=37)
				&& (_defbg>=40 && _defbg<=47));
			cf.crTextColor = def_colors[_deffg-30].v;
			cf.crBackColor = def_colors[_defbg-40].v;
			SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		}
		else if(attr == 1){
			cf.dwMask = CFM_BOLD;
			cf.dwEffects = CFE_BOLD;
			SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		}
		else{
			//debug_out(("unknown or unsupported Linux control format!\n"));
		}

		return true;
	}

	LRESULT c_rich_edit::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled)
	{
		return __super::HandleMessage(uMsg, wParam, lParam, bHandled);
	}

	bool c_rich_edit::get_sel_range(int* start /*= nullptr*/, int* end /*= nullptr*/)
	{
		CHARRANGE rng;
		SendMessage(EM_EXGETSEL, 0, LPARAM(&rng));
		if (start) *start = rng.cpMin;
		if (end)   *end = rng.cpMax;
		return rng.cpMin != rng.cpMax;
	}

	void c_rich_edit::do_copy()
	{
		SendMessage(WM_COPY);
	}

	void c_rich_edit::do_cut()
	{
		SendMessage(WM_CUT);
	}

	void c_rich_edit::do_paste()
	{
		SendMessage(WM_PASTE);
	}

	void c_rich_edit::do_delete()
	{
		SendMessage(WM_CLEAR);
	}

	void c_rich_edit::do_sel_all()
	{
		SendMessage(EM_SETSEL, 0, -1);
	}

	void c_rich_edit::update_timer_period(void)
	{
#if DEBUG_UPDATE_TIMER_PERIOD
		static DWORD ts = 0;
#endif // DEBUG_UPDATE_TIMER_PERIOD
		//(void)stop_replace_timer();
		if (0 < _sz_buffer_usage) {
#ifdef _DEBUG
			if (_sz_buffer_usage > _sz_buffer_max_usage) {
				_sz_buffer_max_usage = _sz_buffer_usage;
			}
#endif // _DEBUG
			debug_printll("periodical flush buffer:%u", _sz_buffer_usage);
#if DEBUG_UPDATE_TIMER_PERIOD
			DWORD nts = GetTickCount();
			printf(_DEBUG_STRING_FILE_LINE_FUNC " %d\t\n", nts - ts);
			ts = nts;
#endif // DEBUG_UPDATE_TIMER_PERIOD
			_buffer[_sz_buffer_usage] = '\0'; // null-terminated string
			_append_text(_buffer, _codepage);
			_sz_buffer_usage = 0;
		}
		//start_replace_timer();
	}

	void c_rich_edit::set_default_text_fgcolor(COLORREF fg)
	{
		CHARFORMAT2 cf;
		::memset(&cf, 0, sizeof(cf));
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_COLOR;
		cf.dwEffects = 0;
		cf.crTextColor = fg;
		SMART_ENSURE(SendMessage(EM_SETCHARFORMAT, 0, LPARAM(&cf)), != 0).Warning();
	}

	void c_rich_edit::set_default_text_bgcolor(COLORREF bg)
	{
		CHARFORMAT2 cf;
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_BACKCOLOR;
		cf.dwEffects = 0;
		cf.crBackColor = bg;
		SMART_ENSURE(SendMessage(EM_SETCHARFORMAT, SCF_ALL, LPARAM(&cf)), != 0).Warning();
	}

	void c_rich_edit::set_default_wnd_bgcolor(COLORREF bg)
	{
		SMART_ENSURE(SendMessage(EM_SETBKGNDCOLOR, (WPARAM)0, (LPARAM)(COLORREF)bg), != 0).Warning();
	}

    void c_rich_edit::limit_text(int sz) {
        // https://msdn.microsoft.com/en-us/library/windows/desktop/bb761647(v=vs.85).aspx
        SendMessage(EM_EXLIMITTEXT, 0, sz == -1 ? 0x7ffffffe : sz);
    }

}
}
