/*
* Copyright 2016 Huy Cuong Nguyen
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "ZXString.h"
#include "ZXUtf16.h"
#include "ZXUtf8.h"

#ifdef NB_HAVE_QT
#include <qstring.h>
#endif

#include <iostream>

namespace ZXing {

namespace {

int CountUtf8Bytes(const uint16_t* utf16, size_t count)
{
	int result = 0;
	for (size_t i = 0; i < count; ++i) {
		uint16_t codePoint = utf16[i];
		if (codePoint < 0x80)
		{
			result += 1;
		}
		else if (codePoint < 0x800)
		{
			result += 2;
		}
		else if (Utf16::IsHighSurrogate(codePoint))
		{
			result += 4;
			++count;
		}
		else
		{
			result += 3;
		}
	}
	return result;
}

int CountUtf8BytesFromUCS2(const uint16_t* utf16, size_t count)
{
	int result = 0;
	for (size_t i = 0; i < count; ++i) {
		uint16_t codePoint = utf16[i];
		if (codePoint < 0x80)
		{
			result += 1;
		}
		else if (codePoint < 0x800)
		{
			result += 2;
		}
		else
		{
			result += 3;
		}
	}
	return result;
}



template <typename Iter>
void stringAppendUtf16(std::string& str, Iter beginRange, Iter endRange)
{
	char buffer[4];
	int bufLength;

	while (beginRange != endRange)
	{
		uint32_t codePoint = *beginRange++;

		if (Utf16::IsHighSurrogate(codePoint) && beginRange != endRange && Utf16::IsLowSurrogate(*beginRange))
		{
			uint32_t nextCodePoint = *beginRange++;
			bufLength = Utf8::Encode(Utf16::CodePointFromSurrogates(codePoint, nextCodePoint), buffer);
		}
		else
		{
			bufLength = Utf8::Encode(codePoint, buffer);
		}
		str.append(buffer, bufLength);
	}
}

template <typename Iter>
void stringAppendUtf32(std::string& str, Iter beginRange, Iter endRange)
{
	char buffer[4];
	int bufLength;
	while (beginRange != endRange)
	{
		bufLength = Utf8::Encode(*beginRange++, buffer);
		str.append(buffer, bufLength);
	}
}

template <typename Container>
void stringToUtf16(const std::string& str, Container& buffer)
{
	typedef typename Container::value_type CharType;

	const uint8_t* src = (const uint8_t*)str.c_str();
	const uint8_t* srcEnd = src + str.size();
	int destLen = Utf8::CountCodePoints((const char*)src);
	if (destLen > 0)
	{
		buffer.reserve(buffer.size() + destLen);
		uint32_t codePoint = 0;
		uint32_t state = Utf8::kAccepted;

		while (src < srcEnd)
		{
			if (Utf8::Decode(*src++, state, codePoint) != Utf8::kAccepted)
			{
				continue;
			}

			if (codePoint > 0xffff) // surrogate pair
			{
				buffer.push_back((CharType)(0xd7c0 + (codePoint >> 10)));
				buffer.push_back((CharType)(0xdc00 + (codePoint & 0x3ff)));
			}
			else
			{
				buffer.push_back((CharType)codePoint);
			}

		}
	}
}

template <typename Container>
void stringToUtf32(const std::string& str, Container& buffer)
{
	const uint8_t* src = (const uint8_t*)str.c_str();
	const uint8_t* srcEnd = src + str.size();
	int destLen = Utf8::CountCodePoints((const char*)src);
	if (destLen > 0)
	{
		buffer.reserve(buffer.size() + destLen);
		uint32_t codePoint = 0;
		uint32_t state = Utf8::kAccepted;

		while (src < srcEnd)
		{
			if (Utf8::Decode(*src++, state, codePoint) != Utf8::kAccepted)
			{
				continue;
			}
			buffer.push_back((typename Container::value_type)codePoint);
		}
	}
}

std::string::const_iterator skipCodePoints(std::string::const_iterator cur, std::string::const_iterator end, int count)
{
	while (cur != end && count > 0)
	{
		if ((*cur & 0x80) == 0)
		{
			++cur;
		}
		else
		{
			switch (*cur & 0xf0)
			{
			case 0xc0: case 0xd0: cur += 2; break;
			case 0xe0: cur += 3; break;
			case 0xf0: cur += 4; break;
			default: // we are in middle of a sequence
				while ((*++cur & 0xc0) == 0x80); break;
			}
		}
		--count;
	}
	return cur;
}

uint32_t readCodePoint(std::string::const_iterator cur, std::string::const_iterator end)
{
	uint32_t codePoint = 0;
	uint32_t state = Utf8::kAccepted;
	while (cur != end)
	{
		switch (Utf8::Decode(*cur, state, codePoint))
		{
		case Utf8::kAccepted:
			return codePoint;
		case Utf8::kRejected:
			return 0xfffd; // REPLACEMENT CHARACTER
		default:
			++cur;
		}
	}
	return 0;
}

};

String::String(const wchar_t* i_wstr) : String(i_wstr, std::char_traits<wchar_t>::length(i_wstr))
{
}

String::String(const wchar_t* i_begin, const wchar_t* i_end) : String(i_begin, i_end - i_begin)
{
}

String::String(const wchar_t* i_wstr, size_t i_len)
{
	if (sizeof(wchar_t) == 2)
	{
		m_utf8.reserve(m_utf8.size() + CountUtf8Bytes(reinterpret_cast<const uint16_t*>(i_wstr), i_len));
		stringAppendUtf16(m_utf8, i_wstr, i_wstr + i_len);
	}
	else
	{
		m_utf8.reserve(m_utf8.size() + Utf8::CountBytes(reinterpret_cast<const uint32_t*>(i_wstr), i_len));
		stringAppendUtf32(m_utf8, i_wstr, i_wstr + i_len);
	}
}

#ifdef NB_HAVE_QT

String::String(const QString& qstr) : String(qstr.utf16()), qstr.length())
{
}

String::operator QString() const
{
	return QString::fromUtf8(m_utf8.c_str(), m_utf8.length());
}

#endif

int
String::charCount() const
{
	return Utf8::CountCodePoints(m_utf8.c_str());
}

void
String::appendUcs2(const uint16_t* ucs2, size_t len)
{
	m_utf8.reserve(m_utf8.size() + CountUtf8BytesFromUCS2(ucs2, len));
	char buffer[4];
	for (size_t i = 0; i < len; ++i)
	{
		int length = Utf8::Encode(static_cast<uint32_t>(ucs2[i]), buffer);
		m_utf8.append(buffer, length);
	}
}

void
String::appendUcs2(const uint16_t* ucs2)
{
	appendUcs2(ucs2, std::char_traits<uint16_t>::length(ucs2));
}

void
String::appendUtf16(const std::vector<uint16_t>& utf16)
{
	m_utf8.reserve(m_utf8.size() + CountUtf8Bytes(utf16.data(), utf16.size()));
	stringAppendUtf16(m_utf8, utf16.begin(), utf16.end());
}

void
String::appendUtf32(const std::vector<uint32_t>& utf32)
{
	m_utf8.reserve(m_utf8.size() + Utf8::CountBytes(utf32.data(), utf32.size()));
	stringAppendUtf32(m_utf8, utf32.begin(), utf32.end());
}

void
String::appendUtf16(const uint16_t* utf16, size_t len)
{
	m_utf8.reserve(m_utf8.size() + CountUtf8Bytes(utf16, len));
	stringAppendUtf16(m_utf8, utf16, utf16 + len);
}

void
String::appendUtf32(const uint32_t* utf32, size_t len)
{
	m_utf8.reserve(m_utf8.size() + Utf8::CountBytes(utf32, len));
	stringAppendUtf32(m_utf8, utf32, utf32 + len);
}

void
String::appendUtf32(uint32_t utf32)
{
	stringAppendUtf32(m_utf8, &utf32, &utf32 + 1);
}

void
String::toUtf16(std::vector<uint16_t>& buffer) const
{
	stringToUtf16(m_utf8, buffer);
}

void
String::toUtf32(std::vector<uint32_t>& buffer) const
{
	stringToUtf32(m_utf8, buffer);
}

void
String::toWString(std::vector<wchar_t>& buffer) const
{
	if (sizeof(wchar_t) == 2)
	{
		stringToUtf16(m_utf8, buffer);
	}
	else
	{
		stringToUtf32(m_utf8, buffer);
	}
}

std::wstring
String::toWString() const
{
	std::wstring buffer;
	if (sizeof(wchar_t) == 2)
	{
		stringToUtf16(m_utf8, buffer);
	}
	else
	{
		stringToUtf32(m_utf8, buffer);
	}
	return buffer;
}

uint32_t
String::charAt(int charIndex) const
{
	return readCodePoint(charIndex > 0 ? skipCodePoints(m_utf8.begin(), m_utf8.end(), charIndex) : m_utf8.begin(), m_utf8.end());
}

String
String::substring(int charIndex, int charCount) const
{
	String result;
	if (charCount != 0)
	{
		auto s = skipCodePoints(m_utf8.begin(), m_utf8.end(), charIndex);
		if (charCount > 0)
		{
			auto e = skipCodePoints(s, m_utf8.end(), charCount);
			return result.m_utf8.assign(s, e);
		}
		result.m_utf8.assign(s, m_utf8.end());
	}
	return result;
}

String::Iterator
String::begin() const
{
	return Iterator(m_utf8.begin(), m_utf8.end());
}

String::Iterator
String::end() const
{
	return Iterator(m_utf8.end(), m_utf8.end());
}

std::ostream &
operator<<(std::ostream& out, const String& str)
{
	return out.write(str.m_utf8.data(), str.m_utf8.size());
}

std::wostream &
operator<<(std::wostream& out, const String& str)
{
	return (out << str.toWString());
}

void
String::Iterator::next()
{
	m_ptr = skipCodePoints(m_ptr, m_end, 1);
}

uint32_t
String::Iterator::read() const
{
	return readCodePoint(m_ptr, m_end);
}

void
String::appendLatin1(const uint8_t* str, size_t len)
{
	int count = 0;
	for (size_t i = 0; i < len; ++i) {
		if (str[i] >= 128) {
			count += 2;
		}
		else {
			count += 1;
		}
	}
	m_utf8.reserve(count);
	stringAppendUtf32(m_utf8, str, str + len);
}

} // ZXing
