#pragma once

namespace Common {
	namespace Window{
		class c_edit : public CWnd
			, public i_timer_period
		{
		public:
			c_edit()
				: _bUseDefMenu(true)
				, _buffer(NULL)
				, _sz_buffer_size(64 * 1024 - 4)
				, _sz_buffer_usage(0)
#ifdef _DEBUG
				, _sz_buffer_max_usage(0)
#endif // _DEBUG
			{
				_buffer = new char[_sz_buffer_size + 4];
				_replace_timer.set_period(40);
				_replace_timer.set_period_timer(this);
				_replace_timer.start();
			}
			virtual ~c_edit(){
				delete[] _buffer;
			}

			virtual LPCTSTR GetSuperClassName() const{return WC_EDIT;}
			virtual LPCTSTR GetWindowClassName() const{return "Common" WC_EDIT;}
			virtual bool ResponseDefaultKeyEvent(HWND hwnd, WPARAM wParam) {return false;}

			void clear() {
				::SetWindowText(*this, ""); 
			}
			virtual bool back_delete_char(int n);
			virtual bool append_text(const char* str, size_t len = 0);
			virtual void set_text(const char* str);
			virtual void update_timer_period(void) override;
			int get_replace_timer(void);
			void set_replace_timer(int ms);
			void start_replace_timer(void);
			int stop_replace_timer(void);

		public: // menu support functions
			virtual bool is_read_only();

		public:
			virtual void limit_text(int sz);

		protected:
			virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled) override;
			HMENU _load_default_menu();
			virtual bool _append_text(const char* str);


		protected:
			bool _bUseDefMenu;
			c_timer _replace_timer;
			char *_buffer;
			size_t _sz_buffer_size;
			size_t _sz_buffer_usage;
#ifdef _DEBUG
			size_t _sz_buffer_max_usage;
#endif // _DEBUG
		};

		class c_rich_edit : public c_edit
		{
		public:
			c_rich_edit()
				: _deffg(30)
				, _defbg(47)
				, _codepage(CP_UTF8)
			{
			}

			~c_rich_edit()
			{
			}

			virtual LPCTSTR GetSuperClassName() const {return RICHEDIT_CLASS;}
			virtual LPCTSTR GetWindowClassName() const{return "Common" RICHEDIT_CLASS;}
			virtual bool back_delete_char(int n);
			//virtual bool append_text(const char* str);
			virtual bool append_text(const char* str, UINT codepage = CP_ACP);
			virtual bool append_byte(const char* str, size_t len, UINT codepage = CP_ACP);
			virtual bool apply_linux_attributes(char* attrs);
			virtual bool apply_linux_attribute_m(int attr);

		public:
            virtual void limit_text(int sz) override;
			void set_default_text_fgcolor(COLORREF fg);
			void set_default_text_bgcolor(COLORREF bg);
			void set_default_wnd_bgcolor(COLORREF bg);
			bool get_sel_range(int* start = nullptr, int* end = nullptr);
			void do_copy();
			void do_cut();
			void do_paste();
			void do_delete();
			void do_sel_all();
			virtual void update_timer_period(void);
			UINT get_cur_codepage(void);
			void set_cur_codepage(UINT codepage);


		protected:
			virtual bool _append_text(const char* str, UINT codepage = CP_ACP);
			virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, bool& bHandled) override;
		
		protected:
			int _deffg;
			int _defbg;
			UINT _codepage;
		};
	}
}
