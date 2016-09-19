#include "stdafx.h"

#include "DataProcessor.h"

namespace Common{
	//////////////////////////////////////////////////////////////////////////
	eProcessType c_hex_data_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		char buf[1024];
		char* str;

        const int line_cch = 16;

		int pos = _count % (line_cch);

		int outlen = cb;
		str = c_text_formatting::hex2str(const_cast<unsigned char*>(ba), &outlen, line_cch, pos,
			buf, sizeof(buf), c_text_formatting::newline_type::NLT_CRLF);

		_editor->append_text(str);

		if (str != buf) memory.free((void**)&str, "");

		_count += cb;

		*pn = cb;
		return kNoMore;
	}

	void c_hex_data_processor::reset_buffer()
	{
		_count = 0;
	}

	void c_hex_data_receiver::receive(const unsigned char* ba, int cb)
	{
		for (; cb > 0;){
			if (_pre_proc){ // 可能处理后cb==0, 所以不管process的返回值
				process(_pre_proc, kMore, &ba, &cb, &_pre_proc);
				continue;
			}

			if (process(_proc_hex, kNoMore, &ba, &cb, &_pre_proc) || cb==0)
				continue;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	eProcessType c_single_byte_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		SMART_ASSERT(_richedit != NULL).Fatal();
		SMART_ASSERT(kNoMore == type).Warning();
		//SMART_ASSERT(cb == 1)(cb).Fatal();

		char buf[5];

		sprintf(buf, "<%02X>", *ba);
		_richedit->append_text(buf);

		*pn = 1;
		return kNoMore;
	}

	//////////////////////////////////////////////////////////////////////////
	eProcessType c_crlf_data_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		char inner_buf[1024];		// 内部缓冲
		int n = 0;					// 记录新crlf的个数
		char* str;

		if (kNoMore == type) {
			_post_len = 0;
			_data.empty();
		}

		while (n < cb && (ba[n] == '\r' || ba[n] == '\n')){
			n++;
		}

		if (n <= 0){
			*pn = 0;
			return kNoMore;
		}

		_data.append(ba, n);

		str = c_text_formatting::hex2chs((unsigned char*)_data.get_data(), _data.get_size(),
			inner_buf, __ARRAY_SIZE(inner_buf), c_text_formatting::NLT_CR);

		do{
			int newcrlen = strlen(str);
			int diff = newcrlen - _post_len;

			if (diff > 0){
				_richedit->append_text(str + _post_len);
			}
			else{
				_richedit->back_delete_char(-diff);
			}
		} while ((0));

		_post_len = strlen(str);

		if (str != inner_buf)
			memory.free((void**)&str, "");

		*pn = n;
		return kMore;
	}

	void c_crlf_data_processor::reset_buffer()
	{
		_post_len = 0;
		_data.empty();
	}

	//////////////////////////////////////////////////////////////////////////
	static int read_integer(char* str, int* pi)
	{
		int r = 0;
		char* p = str;

		while (*p >= '0' && *p <= '9'){
			r *= 10;
			r += *p - '0';
			p++;
		}

		*pi = r;
		return (int)p - (int)str;
	}

	eProcessType c_escape_data_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		int i = 0;
		int step;

		if (kNoMore == type) {
			reset_buffer();
		}

		eProcessType r = kMore;

		for (i = 0; i < cb; i += step){
			step = 0;
			switch (_state)
			{
			case LCS_NONE:
				if (ba[i] == '\033'){
					_state = LCS_ESC;
					step = 1;
					_data.append_char(ba[i]);
				}
				else{
					debug_out(("state: LCS_NONE: expect:\\033, but 0x%02X!\n", ba[i]));
					r = kNoMore;
					goto _exit;
				}
				break;
			case LCS_ESC:
				if (ba[i] == '['){
					_state = LCS_BRACKET;
					step = 1;
					_data.append_char(ba[i]);
				}
				else{
					debug_out(("state: LCS_ESC: expect: [, but 0x%02X\n", ba[i]));
					_state = LCS_NONE;
					r = kNoMore;
					goto _exit;
				}
				break;
			case LCS_BRACKET:
				if (ba[i] >= '0' && ba[i] <= '9'){
					while (i + step<cb && ba[i + step] >= '0' && ba[i + step] <= '9'){
						_data.append_char(ba[i + step]);
						step++;
					}
					_state = LCS_VAL;
				}
				else if (ba[i] == 'H'){
					_data.append_char('H');
					step = 1;
					_state = LCS_H;
				}
				else if (ba[i] == 'f'){
					_data.append_char('f');
					step = 1;
					_state = LCS_f;
				}
				else if (ba[i] == 'm'){
					_data.append_char('m');
					step = 1;
					_state = LCS_m;
				}
				else if (ba[i] == 's'){
					_data.append_char('s');
					step = 1;
					_state = LCS_s;
				}
				else if (ba[i] == 'u'){
					_data.append_char('u');
					step = 1;
					_state = LCS_u;
				}
				else if (ba[i] == 'K'){
					_data.append_char('K');
					step = 1;
					_state = LCS_K;
				}
				else if (ba[i] == 'm'){
					_data.append_char('m');
					step = 1;
					_state = LCS_m;
				}
				else if (ba[i] == '='){
					_data.append_char('=');
					step = 1;
					_state = LCS_EQU;
				}
				else if (ba[i] == 'A'){
					_data.append_char('A');
					step = 1;
					_state = LCS_A;
				}
				else if (ba[i] == 'B'){
					_data.append_char('B');
					step = 1;
					_state = LCS_B;
				}
				else if (ba[i] == 'C'){
					_data.append_char('C');
					step = 1;
					_state = LCS_C;
				}
				else if (ba[i] == 'D'){
					_data.append_char('D');
					step = 1;
					_state = LCS_D;
				}
				else{
					debug_out(("state: LCS_BRACKET: unexpected token: %02X\n", ba[i]));
					_state = LCS_NONE;
					r = kNoMore;
					goto _exit;
				}
				break;
			case LCS_VAL:
				if (ba[i] >= '0' && ba[i] <= '9'){
					while (i + step < cb && ba[i + step] >= '0' && ba[i + step] <= '9'){
						_data.append_char(ba[i + step]);
						step++;
					}
					_state = LCS_VAL;
					goto _no_push_state;
				}
				if (_stack[_stack.size() - 2] == LCS_BRACKET){
					if (ba[i] == ';'){
						_state = LCS_SEMI;
						step = 1;
						_data.append_char(';');
					}
					else if (ba[i] == 'A'){
						_state = LCS_A;
						step = 1;
						_data.append_char('A');
					}
					else if (ba[i] == 'B'){
						_state = LCS_B;
						step = 1;
						_data.append_char('B');
					}
					else if (ba[i] == 'C'){
						_state = LCS_C;
						step = 1;
						_data.append_char('C');
					}
					else if (ba[i] == 'D'){
						_state = LCS_D;
						step = 1;
						_data.append_char('D');
					}
					else if (ba[i] == 'j'){
						_state = LCS_j;
						step = 1;
						_data.append_char('j');
					}
					else if (ba[i] == 'm'){
						_state = LCS_m;
						step = 1;
						_data.append_char('m');
					}
					else{
						debug_out(("state: LCS_VAL: unknown token: %02X\n", ba[i]));
						_state = LCS_NONE;
						r = kNoMore;
						goto _exit;

					}
				}
				else if (_stack[_stack.size() - 2] == LCS_EQU){
					if (ba[i] == 'h'){
						_state = LCS_h;
						step = 1;
						_data.append_char('h');
					}
					else if (ba[i] == 'l'){
						_state = LCS_l;
						step = 1;
						_data.append_char('l');
					}
					else{
						debug_out(("state: LCS_EQU+LCS_VAL: unexpected token: %02X\n", ba[i]));
						_state = LCS_NONE;
						r = kNoMore;
						goto _exit;
					}
				}
				else if (_stack[_stack.size() - 2] == LCS_SEMI){
					if (_stack.size() == 5){
						if (ba[i] == 'H'){
							_state = LCS_H;
							step = 1;
							_data.append_char('H');
							goto _push_state;
						}
						else if (ba[i] == 'f'){
							_state = LCS_f;
							step = 1;
							_data.append_char('f');
							goto _push_state;
						}
					}

					if(ba[i] == ';'){
						_state = LCS_SEMI;
						step = 1;
						_data.append_char(';');
					}
					else if (ba[i] == 'm'){
						_state = LCS_m;
						step = 1;
						_data.append_char('m');
					}
					else{
						debug_out(("state: LCS_SEMI+LCS_VAL: unexpected token: %02X\n", ba[i]));
						_state = LCS_NONE;
						r = kNoMore;
						goto _exit;
					}
				}
				else{
					debug_out(("state: LCS_VAL: unexpected token: %02X\n", ba[i]));
					r = kNoMore;
					goto _exit;
				}
				break;
			case LCS_SEMI:
				if (ba[i] >= '0' && ba[i] <= '9'){
					while (i+step<cb && ba[i + step] >= '0' && ba[i + step] <= '9'){
						_data.append_char(ba[i + step]);
						step++;
					}
					_state = LCS_VAL;
				}
				else if (ba[i] == 'm'){
					_state = LCS_m;
					step = 1;
					_data.append_char('m');
				}
				else if (ba[i] == 'H'){
					_state = LCS_H;
					step = 1;
					_data.append_char('H');
				}
				else if (ba[i] == 'f'){
					_state = LCS_f;
					step = 1;
					_data.append_char('f');
				}
				else if (ba[i] == ';'){
					_state = LCS_SEMI;
					step = 1;
					_data.append_char(';');
				}
				else{
					debug_out(("state: LCS_SEMI: unknown token: %02X\n", ba[i]));
					r = kNoMore;
					goto _exit;
				}
				break;
			case LCS_EQU:
				if (ba[i] >= '0' && ba[i] <= '9'){
					while (i + step < cb && ba[i + step] >= '0' && ba[i + step] <= '9'){
						_data.append_char(ba[i + step]);
						step++;
					}
					_state = LCS_VAL;
				}
				else if (ba[i] == 'h'){
					_state = LCS_h;
					step = 1;
					_data.append_char('h');
				}
				else if (ba[i] == 'l'){
					_state = LCS_l;
					step = 1;
					_data.append_char('l');
				}
				else{
					debug_out(("state: LCS_EQU: unknown token %02X\n", ba[i]));
					r = kNoMore;
					_state = LCS_NONE;
					goto _exit;
				}
				break;
			case LCS_H:	case LCS_f:
			case LCS_A:	case LCS_B:	case LCS_C:	case LCS_D:
			case LCS_s:	case LCS_u:
			case LCS_j:
			case LCS_K:
			case LCS_m:
			case LCS_h:	case LCS_l:
				debug_out(("parsing completed\n"));
				r = kNoMore;
				_state = LCS_NONE;
				_data.append_char('\0');
				_richedit->apply_linux_attributes((char*)_data.get_data());
				goto _exit;
			default:
				debug_out(("unknown lcs token\n"));
				r = kNoMore;
				_state = LCS_NONE;
				goto _exit;
			}
		_push_state:
			_stack.push_back(_state);
		_no_push_state:
			;
		}

	_exit:
		*pn = i;
		return r;
	}

	void c_escape_data_processor::reset_buffer()
	{
		_state = LCS_NONE;
		_stack.clear();
		_stack.push_back(_state);
		_data.empty();
	}

	//////////////////////////////////////////////////////////////////////////
	eProcessType c_ascii_data_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		char buf[1024];
		int n = 0;

		while (n < cb && n<sizeof(buf)-1 && ba[n] >= 0x20 && ba[n] <=0x7F){
			buf[n] = ba[n];
			n++;
		}

		buf[n] = '\0';
		_richedit->append_text(buf);

		*pn = n;
		return kNoMore;
	}

	void c_ascii_data_processor::reset_buffer()
	{

	}

	//////////////////////////////////////////////////////////////////////////
	eProcessType c_unicode_string_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		debug_printll("type:%d, glance:%02X, len:%d", type, *ba, cb);
		typedef union _tag_u8_mem {
			uint64_t mem;
			struct _tag_u8_bytes {
				unsigned char str[6 + 1];
			} u8;
		} u8_mem;

		typedef enum {
			UTF8_OK,
			BROKEN_SEQUENCE, // miss following byte(s)
			INVALID_LEAD,
			INCOMPLETE_SEQUENCE, // lack of byte(s) to verify following byte(s). only use in is_stream_utf8_compliant()
			OVERLONG_SEQUENCE, // combine to INVALID_LEAD
			INVALID_CODE_POINT, // not detect
			UTF8_UNKNOW
		} utf_error;
#define is_valid_lead(ch) (((ch & 0x80) == 0) || (character_depth(ch) > 1 && character_depth(ch) < 7))

		// return value
		// 0    : ascii						-> valid lead (no following byte)
		// 1    : UTF-8 following byte		-> invalid lead (following byte)
		// 2 ~ 6: UTF-8 character length	-> valid lead (have following byte)
		auto character_depth = [](const unsigned char x) {
			if ((x & 0x80) == 0) { //if the left most bit is 0 return 0
				return 0;
			}

			// we don't need to test the left most bit because it was tested above.
			// we should start by testing the second left most bit
			int count;
			for (count = 1; count < 6; count++) { // count: 1 2 3 4 5
				char ch = (x >> (7 - count)) & 0x01; // 7 - count: 6 5 4 3 2
				if (ch == 0) {
					return count;
				}
			}
			return count;
		};

		// return value
		// offset of last valid byte in input stream
		auto is_stream_utf8_compliant = [&character_depth](const unsigned char *stream, int len, utf_error& err) {
			int following_byte_count = 0; // how many following byte(s) should this UTF-8 character have
			int index = 0; // processed count
			int distance_to_lead = 0;

			if (len < 1) {
				debug_puts("stream length is 0");
				err = UTF8_UNKNOW;
				return 0;
			}
			err = UTF8_OK;

			do {
				int is_not_ascii = character_depth(stream[index]);

				if (following_byte_count > 0) { // we expect some byte(s) to form a valid codepoint(s)
					distance_to_lead++;
					if (is_not_ascii == 1) { // what we expect was founded
						following_byte_count--; // Cool, we found next code point, decrease count
						if (0 == following_byte_count) {
							err = UTF8_OK; // finally we found a valid codepoint(s)
						}
						else {
							//err = INCOMPLETE_SEQUENCE; // already done when we found the lead
						}
					}
					else {
						//if (is_not_ascii == 0) {
						// The code point ended early
						err = BROKEN_SEQUENCE; // valid following byte is missing
						/*
						 * character length
						 * LEAD [in]valid byte
						 * init following_byte_count (character length - 1)
						 * except byte count (following_byte_count)
						 * distance_to_lead
						 * index -= 1; // 2+ : Li+    : 1+ : 1+ : 1
						 * index -= 2; // 3+ : Lvi+   : 2+ : 1+ : 2
						 * index -= 3; // 4+ : Lvvi+  : 3+ : 1+ : 3
						 * index -= 4; // 5+ : Lvvvi+ : 4+ : 1+ : 4
						 * index -= 5; // 6  : Lvvvvi : 5  : 1  : 5
						 */
						index -= distance_to_lead; // let down stream handler know where is last valid byte(first invalid codepoint)
						// index -> invalid lead
						break; // we found ascii in a UTF-8 code point location
					}
				}
				else if (following_byte_count == 0) {
					if (is_not_ascii == 0) {
						// this is an ascii character
						// do nothing.
						//err = UTF8_OK;
					}
					else if (is_not_ascii == 1) { // valid lead is missing. this is a UTF-8 following byte
						//if (kMore == type) { // 前面有缓存, 此处应该拿来用?
						//	err = INCOMPLETE_SEQUENCE;
						//	index++; // patch
						//	break;
						//}
						err = INVALID_LEAD; // this should never happen, the beginning of a UTF-8 should begine with a min of 2
						// index -> invalid lead
						break;
					}
					else {
						// valid lead. this codepoint should be [is_not_ascii] depth.
						// so next [is_not_ascii - 1] byte(s) must be UTF-8 following byte
						following_byte_count = is_not_ascii - 1; //This code point count is inclusive of this byte
						err = INCOMPLETE_SEQUENCE; // don't return! just mark we expect some byte(s) to form a valid codepoint
						distance_to_lead = 0;
						// index -> valid lead
						// 如果此处之后 false == (++index < len) 导致退出循环, 需要回退 index 使之指向 lead
					}
				}
				else {
					debug_printl("we should never come here! index=%d, following_byte_count=%d", index, following_byte_count);
					err = UTF8_UNKNOW;
					break;
				}
			} while (++index < len);

			if (err == INCOMPLETE_SEQUENCE) {
				index -= (distance_to_lead + 1); // 第一个数据报的孤独的 lead 应该到这里
				// pos -> invalid lead
			}
			// index -> invalid lead
			return index;
		};

		eProcessType ret = kNoMore;
		int processed = 0;
		unsigned char* stream = (unsigned char*)ba;
#define MAX_INVALID_BYTE_PRINT_BUFFER 4096
#define MAX_INVALID_BYTE_PRINT_COUNT  ((MAX_INVALID_BYTE_PRINT_BUFFER - 1) / 4)
		char print_buffer[MAX_INVALID_BYTE_PRINT_BUFFER];
		char *current_buffer = print_buffer;

		if (kNoMore != type) { // 前面有缓存, 此处应优先处理
			debug_printll("buffered_count=%d", buffered_count);
			utf_error status;
			u8_mem *u8 = static_cast<u8_mem *>(static_cast<void*>(&decode_buffer));
			int pos = is_stream_utf8_compliant(u8->u8.str, buffered_count, status);
			switch (status) {

			case BROKEN_SEQUENCE: // miss following byte(s)
				// 接下来检索此次输入的序列, 看头部是不是following byte, 能不能凑成一个 UTF-8 字符
				debug_printll("BROKEN_SEQUENCE buffer pos=%d", pos);
				break;

			case INVALID_LEAD:
				debug_printll("INVALID_LEAD buffer pos=%d", pos);
				break;

			case INCOMPLETE_SEQUENCE:
				debug_printll("INCOMPLETE_SEQUENCE buffer pos=%d", pos);
				// 重组超时, 此时需要 flush buffer, 然后撒手(返回 false)
				if (kBuffer == type) {
					do {
						int i = 0;
						do { // handle invalid byte(s) untile we meet a valid lead
							 // the first byte is invalid so we can use do{}while() loop
							int printed = sprintf(current_buffer, "<%02X>", u8->u8.str[i]);
							if (printed != -1) {
								current_buffer += printed;
							}
							else {
								debug_printll("sprintf failed, %d, %p", i, current_buffer);
								break;
							}
							i++;
						} while (i < buffered_count);
						current_buffer[i] = 0; // null-terminated
						reset_buffer();
						debug_printll("timeout:flush %d bytes: \"%s\"", buffered_count, print_buffer);
						_richedit->append_text(print_buffer, CP_UTF8);
					} while (0);
					*pn = 0;
					return kNoMore;
				}

				do {
					int unprocessed = cb - processed;
					int i = 0;
					do {
						// handle invalid byte(s) untile we meet a valid lead or '\0'
						// the first byte is valid lead
						u8->u8.str[i + buffered_count] = stream[i];
						i++;
					} while (!is_valid_lead(stream[i]) && i < unprocessed);
					//memcpy(u8->u8.str + buffered_count, stream, unprocessed);
					buffered_count += i;
					processed += i;
					stream += i;
					cb -= i;
					debug_printll("%d bytes eaten, new cb:%d", i, cb);
				} while (0);
				// is valid codepoint?
				//int is_not_ascii = character_depth(u8->u8.str);
				pos = is_stream_utf8_compliant(u8->u8.str, buffered_count, status);
				if (UTF8_OK != status) {
					debug_printll("codepoint not buffered, remain %d bytes", cb);
					ret = kMore; // request to be next handle
					break;
				}
				else {
					debug_puts("jump to case UTF8_OK");
				}

			case UTF8_OK: // 理想状况是缓存的字节刚好凑成一个 UTF-8 字符
				debug_printll("codepoint buffered, remain %d bytes", cb);
				_richedit->append_text((const char*)u8->u8.str, CP_UTF8);
				reset_buffer();
				break;

			case UTF8_UNKNOW:
				debug_puts("UTF8_UNKNOW buffer");
				break;
			}
		}

		do {
			if (0 == cb) {
				debug_printll("buffer eaten %d", processed);
				break;
			}
			utf_error status;
			int pos = is_stream_utf8_compliant(stream, cb - processed, status);

			switch (status) {

			case BROKEN_SEQUENCE:
				debug_printll("BROKEN_SEQUENCE pos=%d", pos);
				// miss following byte(s)
				// pos -> invalid lead
				// [valid UTF-8 character]+, valid lead, byte* (lack of following byte), valid lead
				// treat as invalid untile we meet next valid lead
				// pos points to lead of broken codepoint
				if (pos > 0) {
					// handle leading valid codepoint(s)
					unsigned char backup = stream[pos];
					stream[pos] = 0;
					_richedit->append_text((const char*)stream, CP_UTF8);
					stream[pos] = backup;
					processed += pos;
					stream += pos;
					pos = 0; // make sure later code handle invalid byte(broken codepoint)
				}
				debug_puts("jump to case INVALID_LEAD");
				// don't break here!
			case INVALID_LEAD:
				// pos -> invalid lead
				// invalid lead, byte*, [valid lead (from case INVALID_LEAD)]
				// valid lead, byte*, [valid lead (from case BROKEN_SEQUENCE)] -> the leading valid lead will be treat as invalid byte
				if (pos == 0) {
					// handle broken codepoint
					do {
						int i = 0;
						do { // handle invalid byte(s) untile we meet a valid lead
							// the first byte is invalid so we can use do{}while() loop
							int printed = sprintf(current_buffer, "<%02X>", stream[i]);
							if (printed != -1) {
								current_buffer += printed;
							}
							else {
								debug_printll("sprintf failed, %d, %p", i, current_buffer);
#ifdef _DEBUG
								break; // incase of sprintf failed
#endif
							}
							i++;
						} while (!is_valid_lead(stream[i]) && i < MAX_INVALID_BYTE_PRINT_COUNT && !(processed + i > cb));
						current_buffer[i] = 0; // null-terminated
						current_buffer = print_buffer; // reset buffer pointer
						_richedit->append_text(print_buffer, CP_UTF8);
						processed += i;
						stream += i;
						continue; // it's time to handle valid codepoint(s)
					} while (0);
				}
				break;

			case UTF8_OK:
				// pos -> valid lead (from case UTF8_OK)
				_richedit->append_text((const char*)stream, CP_UTF8);
				processed += pos;
				stream += pos;
				debug_printll("perfect u8, %d bytes", cb);
				break;

			case INCOMPLETE_SEQUENCE:
				// incomplete tail
				debug_printll("INCOMPLETE_SEQUENCE stream pos=%d", pos);
				if (pos > 0) {
					// handle leading valid codepoint(s)
					unsigned char backup = stream[pos];
					stream[pos] = 0;
					_richedit->append_text((const char*)stream, CP_UTF8);
					stream[pos] = backup;
					processed += pos;
					stream += pos;
					pos = 0; // make sure later code handle invalid byte(broken codepoint)
				}
				// buffer incomplete tail byte(s) before return
				/*
				 * 既然 *pn 返回的已处理字节数不等于输入的 cb 就会触发c_text_data_receiver::receive循环
				 * 从而继续调用本函数, 那么是否可以不缓存剩余的这些字节片段呢？
				 * *** 这里缓存未完整到达的字符应该是为了等下一条读数据消息到来后再尝试拼接在一起, 所以上面的代码需要执行拼接工作
				 */
				/*else if (pos == 0)*/ { // most likely when runing in low baudrate such like 9600
					int unprocessed = cb - processed;
					debug_printll("processed %d bytes", processed);
#ifdef _DEBUG
					if (unprocessed > 5) { // should be BROKEN_SEQUENCE
						debug_puts("should be BROKEN_SEQUENCE, not INCOMPLETE_SEQUENCE");
					}
					else
#endif
					{
						debug_printll("buffer incomplete tail %d bytes", unprocessed);
						u8_mem *u8 = static_cast<u8_mem *>(static_cast<void*>(&decode_buffer));
						int i = 0;
						do {
							// handle invalid byte(s) untile we meet a valid lead or '\0'
							// the first byte is valid lead
							u8->u8.str[i] = stream[i];
							i++;
						} while (!is_valid_lead(stream[i]) && i < unprocessed);
						//memcpy(u8->u8.str + buffered_count, stream, unprocessed);
						buffered_count += i;
						processed += i;
						//stream += i; // unnecessary, because we will break and than return
					}
				}
				ret = kMore; // request to be next handle
				break;

			case UTF8_UNKNOW:
				debug_puts("UTF8_UNKNOW stream");
				break;
			}
		} while (0);

		*pn = processed;
		// shou be an timeout for reassemble codepoint. ie. an edit box at UI to specify validity interval
		// before process_some() return, check buffered_count to see if there is any buffered byte.
		// we should inform caller that validity interval is counting.
		if (buffered_count) {
			// inform caller(c_text_data_receiver::receive) that validity interval should be count.
			;
		}
		debug_printll("ret:%d", ret);
		return ret;
	}

	void c_unicode_string_processor::reset_buffer()
	{
		decode_buffer = 0;
		buffered_count = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	void c_text_data_receiver::update_timer_period()
	{
		(void)stop_validity_ticker();
		debug_printll("_pre_proc:%p, validity:%d", _pre_proc, get_validity_interval());
		if (NULL != _pre_proc) {
			// 0xC0000005: 读取位置 0x00000000 时发生访问冲突。
			//eProcessType more = process(_pre_proc, kBuffer, NULL, NULL, &_pre_proc);
			unsigned char dummy = '\0';
			int cb = 0;
			const unsigned char* ba = &dummy;
			eProcessType more = process(_pre_proc, kBuffer, &ba, &cb, &_pre_proc);
			if (more > kNoMore) {
				// 启动超时计数, 结束时调用 _pre_proc
				start_validity_ticker();
			}
			else {
				stop_validity_ticker();
			}
		}
	}

	void c_text_data_receiver::receive(const unsigned char* ba, int cb)
	{
		for (; cb > 0;){
			debug_printll("glance:%02X, len:%d, _pre_proc:%p", *ba, cb, _pre_proc);
			if (_pre_proc){// 可能处理后cb==0, 所以不管process的返回值
				// c_*_processor::process_some 返回真才能满足上一行的 if 判断从而执行到这里
				eProcessType more = process(_pre_proc, kMore, &ba, &cb, &_pre_proc);
				if (more > kNoMore) {
					// 启动超时计数, 结束时调用 _pre_proc
					debug_printll("start_validity_ticker for:%p", _pre_proc);
					start_validity_ticker();
				}
				continue;
			}

			// 控制字符/特殊字符处理
			if (0 <= *ba && *ba <= 0x1F){
				// Bell, 响铃, 7
				if (*ba == 7){
					::Beep(800, 300);
					ba += 1;
					cb -= 1;
				}
				// 删除(退格), 8
				else if (*ba == '\b'){
					int n = 0;
					while (n < cb && ba[n] == '\b') n++;
					_rich_editor->back_delete_char(n);
					ba += n;
					cb -= n;
				}
				// 水平制表, 9
				else if (*ba == '\t'){
					int n = 0;
					char buf[64];
					while (n < cb && n < sizeof(buf) - 1 && ba[n] == '\t'){
						buf[n] = '\t';
						n++;
					}

					buf[n] = '\0';
					_rich_editor->append_text(buf);

					ba += n;
					cb -= n;
				}
				// 回车与换行 (13,10)
				else if (*ba == '\r' || *ba == '\n'){
					process(_proc_crlf, kNoMore, &ba, &cb, &_pre_proc);
				}
				// Linux终端, nCursors 控制字符处理
				else if (*ba == '\033'){
					process(_proc_escape, kNoMore, &ba, &cb, &_pre_proc);
				}
				// 其它 未作处理/不可显示 字符处理
				else{
					process(_proc_byte, kNoMore, &ba, &cb, &_pre_proc);
				}
			}
			// 空格以后的ASCII标准字符处理
			else if (0x20 <= *ba && *ba <= 0x7F){
				process(_proc_ascii, kNoMore, &ba, &cb, &_pre_proc);
			}
			// 扩展ASCII(Extended ASCII / EUC)字符处理
			else{
				eProcessType more = process(_char_decoder, kNoMore, &ba, &cb, &_pre_proc);
				if (more > kNoMore) {
					// 启动超时计数, 结束时调用 _pre_proc
					debug_printll("start_validity_ticker for:%p", _pre_proc);
					start_validity_ticker();
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 关于 gb2312
	// 低字节:
	//		01-94位, 全部使用, 范围: 0xA1->0xFE
	// 高字节:
	//		01-09区为特殊符号: 范围: 0xA1->0xA9
	//		10-15区: 未使用, 范围: 0xAA->0xAF
	//		16-55区: 一级汉字, 按拼音排序, 范围: 0xB0->0xD7
	//		56-87区: 二级汉字, 按部首/笔画排序, 范围: 0xD8->0xF7
	//		88-94区: 未使用, 范围: 0xF8->0xFE
	//    
	// 关于中文处理的正确性保证:
	// 串口设备由于协议的某种不完整性, 很难保证数据总是完全无误,
	// 如果在处理过程中遇到错误的编码就很难显示出正确的中文了, 包括后续的字符, 可能导致一错多错
	eProcessType c_gb2312_data_processor::process_some(eProcessType type, const unsigned char* ba, int cb, int* pn)
	{
		debug_printll("%d, %d, %X", type, cb, *ba);
		// 是否继续上一次未完的处理?
		if (kMore == type) {
			unsigned char chs[16];

			if (*ba >= 0xA1 && *ba <= 0xFE){
				// 有编码区
				if (0xA1 <= _lead_byte && _lead_byte <= 0xAF
					|| 0xB0 <= _lead_byte && _lead_byte <= 0xF7)
				{
					chs[0] = _lead_byte;
					chs[1] = *ba;
					chs[2] = '\0';
					_richedit->append_text((const char*)chs);
				}
				// 无编码区
				else{
					sprintf((char*)chs, "<A+%02X%02X>", _lead_byte, *ba);
					_richedit->append_text((const char*)chs);
				}


				_lead_byte = 0;
				*pn = 1;
				return kNoMore;
			}
			else{
				// 这里该如何处理是好? 
				// 返回1?
				// 还是把_lead_byte和当前字节显示了?
				sprintf((char*)chs, "<%02X>", _lead_byte);
				_richedit->append_text((const char*)chs);

				_lead_byte = 0;
				*pn = 0;
				return kNoMore;
			}
		}
		// 开始新的处理
		else{
			_lead_byte = 0;

			const int kPairs = 512;	// 一次最多处理512个中文字符
			int npairs = 0;			// 有多少对中文需要处理
			char buf[kPairs*2+1];

			// 先把能处理的处理掉, 其它的交给下一次处理
			while (npairs <= kPairs && (npairs + 1) * 2 <= cb){	// 处理第npairs对时要求的字节数, 从0开始, 比如3: 需要至少8个字节
				unsigned char b1 = ba[npairs * 2 + 0];
				unsigned char b2 = ba[npairs * 2 + 1];
				if ((0xA1 <= b1 && b1 <= 0xFE) && (0xA1 <= b2 && b2 <= 0xFE)){
					npairs++;
				}
				else{
					break;
				}
			}

			if (npairs){
				// BUG: 未处理非编码区
				::memcpy(buf, ba, npairs * 2);
				buf[npairs * 2] = '\0';
				_richedit->append_text(buf);

				*pn = npairs * 2;
				return kNoMore;
			}
			else{
				// 只存在一个字节满足的情况
				// 或是当前中剩下一个字节需要处理
				if (cb < 2){ // only can equal to 1
					SMART_ASSERT(cb == 1)(cb).Fatal();
					_lead_byte = *ba;
					*pn = 1;
					return kMore;
				}
				else{
					sprintf(buf, "<%02X>", ba[0]);
					_richedit->append_text(buf);

					*pn = 1;
					return kNoMore;
				}
			}
		}
	}

	void c_gb2312_data_processor::reset_buffer()
	{
		_lead_byte = 0;	//中文前导不可能为零, 所以这样做是完全没问题的
	}
}
