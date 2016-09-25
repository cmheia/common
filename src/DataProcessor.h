#pragma once

namespace Common{
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	typedef enum tag_eProcessType {
		kNoMore = 0,	// 无须再次处理
		kMore,			// 需要再次处理
		kBuffer			// 有缓存, 字节超时
	} eProcessType;

	// 数据处理器接口: 比如文本管理器 16进制管理器, 由下面的数据接收器调用
	// 一般由需要有后续处理的数据处理继承此接口, 否则可以直接处理, 比如'\t'的处理就不需要
	class i_data_processor
	{
	public:
		// 处理部分数据: 
		//		type:	详见 eProcessType
		//		ba:		Byte Array, 字节数组
		//		cb:		Count of Bytes, 字节数
		//		*pn:	本次处理了多少数据
		// 返回值:
		//		eProcessType
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn) = 0;

		// 重置数据处理缓冲: 比如, 在关闭串口后, 或清空16进制数据后
		virtual void reset_buffer() = 0;

		virtual operator i_data_processor*() = 0;
	};

	// 数据接收器接口: 串口在接收到数据后调用所有的接收器
	class i_data_receiver
	{
	public:
		// 数据接收函数, 读线程接收到数据时调用此函数
		// ba指向的内容不应该被更改!
		virtual void receive(const unsigned char* ba, int cb) = 0;
		virtual void reset_buffer() = 0;
	protected:
		// 一个调用处理器并设置剩余数据与后续调用标志的辅助函数
		// type: 详见 eProcessType
		virtual eProcessType process(i_data_processor* proc, eProcessType type, const unsigned char** pba, int* pcb, i_data_processor** ppre)
		{
			int n;
			eProcessType c = proc->process_some(type, *pba, *pcb, &n);
			SMART_ASSERT(n <= *pcb)(n)(*pcb).Fatal();
			*pba += n;
			*pcb -= n;
			*ppre = (c != kNoMore) ? proc : NULL;
			return c;
		}
	};

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	class c_single_byte_processor : public i_data_processor
	{
	public:
		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer() {}

	public:
		Window::c_rich_edit*	_richedit;
	};

	class c_crlf_data_processor : public i_data_processor
	{
	public:
		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer();

		c_crlf_data_processor()
			: _post_len(0)
		{}

	protected:
		int _post_len;
		c_byte_array<16, 64> _data;
	public:
		Window::c_rich_edit* _richedit;
	};

	// Linux控制字符处理
	// http://www.cnblogs.com/memset/p/linux_printf_with_color.html
	// http://ascii-table.com/ansi-escape-sequences.php
	class c_escape_data_processor : public i_data_processor
	{
	public:
		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer();

	protected:
		enum lcs_state{
			LCS_NONE,LCS_ESC,LCS_BRACKET,LCS_VAL,LCS_SEMI,
			LCS_H,LCS_f,
			LCS_A,LCS_B,LCS_C,LCS_D,
			LCS_s,LCS_u,LCS_j,LCS_K,
			LCS_h,LCS_l,LCS_EQU,
			LCS_m,LCS_P
		} _state;
		c_byte_array<64, 64> _data;		// 数据栈
		std::vector<lcs_state> _stack;	// 状态栈
	public:
		Window::c_rich_edit* _richedit;
	};

	class c_ascii_data_processor : public i_data_processor
	{
	public:
		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer();

	public:
		Window::c_rich_edit* _richedit;
	};

	class c_unicode_string_processor : public i_data_processor
	{
	public:
		c_unicode_string_processor()
			: decode_buffer(0), buffered_count(0)
		{}
		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer();

	public:
		uint64_t decode_buffer;
		int buffered_count;
		Window::c_rich_edit* _richedit;
	};

	// 中文扩展ASCII字符处理 (CodePage936 compatible, EUC-CN)
	// http://en.wikipedia.org/wiki/GB_2312
	// http://zh.wikipedia.org/wiki/GB_2312
	// http://www.knowsky.com/resource/gb2312tbl.htm
	class c_gb2312_data_processor : public i_data_processor
	{
	public:
		c_gb2312_data_processor()
			: _lead_byte(0)
		{}
		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer();

	public:
		unsigned char _lead_byte;			// 中文前导字符
		Window::c_rich_edit* _richedit;
	};

	//////////////////////////////////////////////////////////////////////////
	class c_text_data_receiver : public i_data_receiver
		, public i_timer_period
	{
	public:
		c_text_data_receiver()
			: _pre_proc(NULL)
			, _rich_editor(NULL)
			, _char_decoder(_proc_unicode)
		{
			_validity_timer.set_period(100);
			_validity_timer.set_period_timer(this);
			debug_printll("_proc_ascii:%p", (void *)_proc_ascii);
			debug_printll("_proc_byte:%p", (void *)_proc_byte);
			debug_printll("_proc_crlf:%p", (void *)_proc_crlf);
			debug_printll("_proc_escape:%p", (void *)_proc_escape);
			debug_printll("_proc_gb2312:%p", (void *)_proc_gb2312);
			debug_printll("_proc_unicode:%p", (void *)_proc_unicode);
		}

		enum character_encoding_e {
			charset_gb2312 = 0,
			charset_utf8,
		};
		typedef struct {
			character_encoding_e id;
			char *name;
			i_data_processor* processor;
		} encoding_t;

		// interface i_data_receiver
		virtual void receive(const unsigned char* ba, int cb);
		virtual void reset_buffer(){
			_pre_proc = NULL;
			_proc_ascii.reset_buffer();
			_proc_unicode.reset_buffer();
			_proc_escape.reset_buffer();
			_proc_crlf.reset_buffer();
			_proc_gb2312.reset_buffer();
		}
		void set_editor(Window::c_rich_edit* edt) {
			_rich_editor = edt;
			_proc_byte._richedit = edt;
			_proc_ascii._richedit = edt;
			_proc_unicode._richedit = edt;
			_proc_escape._richedit = edt;
			_proc_crlf._richedit = edt;
			_proc_gb2312._richedit = edt;
		}
		void set_validity_interval(int ms) {
			_validity_timer.set_period(ms);
		}
		int get_validity_interval(void) {
			return _validity_timer.get_period();
		}
		void start_validity_ticker() {
			stop_validity_ticker();
			_validity_timer.start();
		}
		int stop_validity_ticker(void) {
			if (_validity_timer.is_running()) {
				_validity_timer.stop();
			}
			return _validity_timer.get_period();
		}
		const encoding_t* get_encoding_list() {
			return &_encoding_list[0];
		}
		int get_encoding_list_len() {
			return (sizeof(_encoding_list) / sizeof(_encoding_list[0]));
		}
		const char* encoding_id_2_name(character_encoding_e id) {
			return _encoding_list[id].name;
		}
		const character_encoding_e encoding_name_2_id(const char* name) {
			for (int i = 0; i < get_encoding_list_len(); i++) {
				if (0 == strcmp(name, _encoding_list[i].name)) {
					return _encoding_list[i].id;
				}
			}
			return _encoding_list[0].id;
		}
		void set_char_encoding(character_encoding_e en) {
			_char_decoder = _encoding_list[en].processor;
			debug_printll("encoding:%s, decoder:%p", encoding_id_2_name(en), _char_decoder);
		}
		void set_char_timeout(int timeout) {
			set_validity_interval(timeout);
		}

	protected:
		Window::c_rich_edit*		_rich_editor;
		i_data_processor*			_pre_proc;
		c_single_byte_processor		_proc_byte;
		c_crlf_data_processor		_proc_crlf;
		c_escape_data_processor		_proc_escape;
		c_ascii_data_processor		_proc_ascii;
		c_unicode_string_processor	_proc_unicode;
		c_gb2312_data_processor		_proc_gb2312;
		c_timer						_validity_timer;
		virtual void update_timer_period() override;
		const encoding_t _encoding_list[2] = {
			{ charset_gb2312, "GB2312", _proc_gb2312 },
			{ charset_utf8, "UTF-8", _proc_unicode },
		};
		i_data_processor*			_char_decoder;
	};

	//////////////////////////////////////////////////////////////////////////
	class c_hex_data_processor : public i_data_processor
	{
	public:
		c_hex_data_processor()
			: _editor(0)
			, _count(0)
		{}

		virtual operator i_data_processor*() { return static_cast<i_data_processor*>(this); }
		virtual eProcessType process_some(eProcessType type, const unsigned char* ba, int cb, int* pn);
		virtual void reset_buffer();
		void set_count(int n) { _count = n; }

		Window::c_edit*			_editor;

	private:
		int _count;
	};

	class c_hex_data_receiver : public i_data_receiver
	{
	public:
		c_hex_data_receiver()
			: _pre_proc(0)
			, _editor(0)
		{}

		virtual void receive(const unsigned char* ba, int cb);
		virtual void reset_buffer(){
			_pre_proc = 0;
			_proc_hex.reset_buffer();
		}
		void set_editor(Window::c_edit* edt) {
			_editor = edt;
			_proc_hex._editor = edt;
		}

		void set_count(int n){ _proc_hex.set_count(n); }
	protected:
		Window::c_edit*			_editor;
		i_data_processor*		_pre_proc;
		c_hex_data_processor	_proc_hex;
	};

	//////////////////////////////////////////////////////////////////////////
	class c_file_data_receiver : public i_data_receiver {
	public:
		c_file_data_receiver()
		{}

		virtual void receive(const unsigned char* ba, int cb) {
			_data.append(ba, cb);
		}
		virtual void reset_buffer() {
			_data.empty();
		}

		size_t size() { 
			return _data.get_size(); 
		}
		unsigned char* data() {
			return reinterpret_cast<unsigned char*>(_data.get_data());
		}
	protected:
		c_byte_array<1 << 20, 1 << 20> _data;
	};
}
