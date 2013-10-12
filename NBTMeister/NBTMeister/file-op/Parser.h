/*
 * Copyright (c) 2013, Marc-André Brochu AKA Mister Guacamole
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include "../config.h"
#include "../make_unique.h"
#include "../tags/Tag.h"

using namespace std;

typedef vector<char> memblock; // used for storing binary data
typedef void (*feedback_fct)(double); // function pointer used to send feedback when building the tree

// an enum for the error states of the parser
enum parser_status {
	good,
	range_illegal,
	malformed_stream,
	null_iterator,
	what_the_fuck
};

/*
 ------------------------------------------------------
 ------------------------------------------------------
 ABOUT THE IMPLEMENTATION
 ------------------------------------------------------
 This class can parse a uncompressed NBT structure and construct a tree from it.
 */
class Parser {
	
	public:
		Parser() : m_status(good) {}
	
		Tag *build(memblock::iterator cursor, memblock::iterator end, feedback_fct feedback = nullptr) {
			return m_build(cursor, end, cursor, feedback);
		}
	
		// returns the 'status' of the parser
		parser_status status() { return m_status; }
	
	private:
		typedef char byte;
		parser_status m_status;
	
		// this function builds a tree from the data passed.
		// it creates an object on the heap, so don't forget to free it!
		// you need to pass the range in which the parser will do its job
		// About the reassign parameter: its purpose is to set the external iterator the same as the internal iterator while recursion
		Tag *m_build(memblock::iterator cursor, memblock::iterator end, memblock::iterator &reassign, feedback_fct feedback = nullptr) {
			
			// we make sure the begin is not greater than the end
			if (cursor - end > 0) {
				m_status = range_illegal;
				return nullptr; // return NULL. error checking is your friend
			}
			
			// we parse all of the data, one byte at time
			while (true) {
				
				// -----------------------------------------------------------------------------------------
				// STEP 1
				// Since this is the 'Named Binary Tag' format, we expect the first byte of the stream to be
				// a tag type. If its not, we set the status to 'malformed_stream' and we return NULL.
				// Once we know what the type of the tag is, we read the following 2 bytes to get the lenght of the
				// name of the tag. Then, we read that many bytes to read the actual name, and we store it.
				// If we reach end-of-stream too early, status is set to null_iterator and we return NULL.
				// Everything is in big-endian, we want to force little-endian (if present in the config file)
				TagType tagType = static_cast<TagType>(*cursor);
				if (tagType >= TagTypeCount || tagType < 0) {
					m_status = malformed_stream;
					return nullptr;
				}
				else if (tagType == TagTypeEnd)
					// this is a TagEnd. When this happens, we return nullptr because it means that either:
					// 	1) we are at the end of an array and we need to signal the upper level that the subfunction has finished
					//	2) there has been an error in the stream
					return nullptr;
				
				// get the length of the name
				SINGLE_GETSHORT tagNameLength;
				m_secureIncrement(cursor, end, reassign);
				vector<byte> buff = m_makeBuffer<typeof(tagNameLength)>(cursor, end, reassign);
				if (m_status != good)
					return nullptr;
				m_rehostEndianness<typeof(tagNameLength)>(tagNameLength, buff);
				
				// we finally get the name of the tag
				string tagName;
				for (int i = 0; i < tagNameLength; i++) {
					
					m_secureIncrement(cursor, end, reassign); // fetch the next byte
					if (m_status != good) // an error has occured
						return nullptr;
					
					// everything is ok, we can get the byte
					tagName += *cursor;
				}
				
				
				// -----------------------------------------------------------------------------------------
				// STEP 2
				// We now have the type and the name of the tag. These are shared amongst Singles and Arrays,
				// but now we have to split the code into 2 parts, one if the tag is a Single and another for
				// if the tag is an Array. Yes, it's time to get the motherfreaking payload!
				//
				// The method for getting the payload differt immensely from tag type to tag type, so we can't
				// have a completely generalized method for this. We need to make different blocks of code for
				// almost each tag type. For example, if the type is an array, we want to call this function,
				// making it recursive, to get the content of the compound/list. If the tag is, lets say, a
				// string, we need to process the payload completely differently : first we read 2 bytes, getting
				// the length of the real payload. We can then read it in its entirety.
				m_secureIncrement(cursor, end, reassign);
				if (m_status != good) // an error has occured
					return nullptr;
				
				return m_readPayload(tagType, tagName, cursor, end, reassign);
			}
			
			return nullptr;
		}
	
	
	
		////////////////////////////////////////////////////////////////////////
		// function that will read only the payload of the specified tag type //
		////////////////////////////////////////////////////////////////////////
		Tag *m_readPayload(TagType tagType, const string &tagName, memblock::iterator &cursor, memblock::iterator end, memblock::iterator &reassign) {
			
			
			// ====================================================================
			// ====================================================================
			// ====================================================================
			// BOOKMARK: List & Compound
			if (tagType == TagTypeList || tagType == TagTypeCompound) {
				
				if (tagType == TagTypeList) { // if this is a list, we need to get 1 byte for the type and the length
					
					// we get the type
					TagType listTagType = static_cast<TagType>(*cursor);
					m_secureIncrement(cursor, end, reassign);
					
					
					// we get the length of the list
					SINGLE_GETINT tagPayloadLength;
					typedef typeof(tagPayloadLength) mtype;
					vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
					if (m_status != good)
						return nullptr;
					m_rehostEndianness<mtype>(tagPayloadLength, buff);
					
					
					// we get the actual tags in the list.
					// These tags are unnamed; they only contains their payload
					Array *root = new Array(tagName, ArrayType::List, listTagType);
					for (int i = 0; i < tagPayloadLength; i++) {
						
						m_secureIncrement(cursor, end, reassign);
						if (m_status != good) // an error has occured
							return nullptr;
						
						Tag *ret = m_readPayload(listTagType, "", cursor, end, reassign);
						root->addTag(ret);
					}
					return root;
				}
				else {
					
					Array *root = new Array(tagName, ArrayType::Compound);
					while (Tag *ret = m_build(cursor, end, cursor)) { // Recursion. This will get us a root tag to add to our array
						root->addTag(ret);
						m_secureIncrement(cursor, end, reassign);
					}
					return root;
				}
			}
			
			
			
			// ====================================================================
			// ====================================================================
			// ====================================================================
			// BOOKMARK: String
			else if (tagType == TagTypeString) { // we read 2 bytes to get the length, then that number of bytes
				
				// we get the necessary number of bytes and we transform them into
				// the desired type
				SINGLE_GETSHORT tagPayloadLength;
				typedef typeof(tagPayloadLength) mtype;
				vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
				if (m_status != good)
					return nullptr;
				m_rehostEndianness<mtype>(tagPayloadLength, buff);
				
				string tagPayload;
				for (int i = 0; i < tagPayloadLength; i++) {
					
					m_secureIncrement(cursor, end, reassign); // fetch the next byte
					if (m_status != good) // an error has occured
						return nullptr;
					
					// everything is ok, we can get the byte
					tagPayload += *cursor;
				}
				
				return new Single(tagName, tagPayload);
			}
			
			
			
			// ====================================================================
			// ====================================================================
			// ====================================================================
			// BOOKMARK: Byte & Int arrays
			else if (tagType == TagTypeByteArray || tagType == TagTypeIntArray) { // we need to read 4 bytes to get the length
				
				
				SINGLE_GETINT tagPayloadLength;
				typedef typeof(tagPayloadLength) mtype;
				vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
				if (m_status != good)
					return nullptr;
				m_rehostEndianness<mtype>(tagPayloadLength, buff);
				
				if (tagType == TagTypeByteArray) { // we need to read 'size' bytes
					
					vector<SINGLE_GETBYTE> tagPayload;
					for (int i = 0; i < tagPayloadLength; i++) {
						
						m_secureIncrement(cursor, end, reassign);
						if (m_status != good) // an error has occured
							return nullptr;
						tagPayload.push_back(*cursor);
					}
					
					return new Single(tagName, tagPayload);
				}
				else { // we need to read 'size' * 4 bytes (we are reading int's)
					
					vector<SINGLE_GETINT> array;
					for (int i = 0; i < tagPayloadLength; i++) {
						
						SINGLE_GETINT tagPayload;
						typedef typeof(tagPayload) mtype;
						vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
						if (m_status != good)
							return nullptr;
						m_rehostEndianness<mtype>(tagPayload, buff);
						
						array.push_back(tagPayload);
					}
					
					return new Single(tagName, array);
				}
			}
			
			
			
			// ====================================================================
			// ====================================================================
			// ====================================================================
			// BOOKMARK: Other tags
			else { // we only need to read the length that goes with the type (int = 4, short = 2, byte = 1, etc.)
				
				switch (tagType) {
						
						
					// ====================================================================
					// ====================================================================
					case TagTypeByte: { // we read 1 byte
						byte tagPayload = *cursor;
						if (m_status != good) // an error has occured
							return nullptr;
						return new Single(tagName, SINGLE_BYTE(tagPayload));
					}
						
						
					// ====================================================================
					// ====================================================================
					case TagTypeShort: {
						
						// we get the necessary number of bytes and we transform them into
						// the desired type
						SINGLE_GETSHORT tagPayload;
						typedef typeof(tagPayload) mtype;
						vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
						if (m_status != good)
							return nullptr;
						m_rehostEndianness<mtype>(tagPayload, buff);
						
						return new Single(tagName, SINGLE_SHORT(tagPayload));
					}
						
						
					// ====================================================================
					// ====================================================================
					case TagTypeInt: {
						
						// we get the necessary number of bytes and we transform them into
						// the desired type
						SINGLE_GETINT tagPayload;
						typedef typeof(tagPayload) mtype;
						vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
						if (m_status != good)
							return nullptr;
						m_rehostEndianness<mtype>(tagPayload, buff);
						
						return new Single(tagName, SINGLE_INT(tagPayload));
					}
					
						
					// ====================================================================
					// ====================================================================
					case TagTypeLong: {
						
						// we get the necessary number of bytes and we transform them into
						// the desired type
						SINGLE_GETLONG tagPayload;
						typedef typeof(tagPayload) mtype;
						vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
						if (m_status != good)
							return nullptr;
						m_rehostEndianness<mtype>(tagPayload, buff);
						
						return new Single(tagName, SINGLE_LONG(tagPayload));
					}
						
						
					// ====================================================================
					// ====================================================================
					case TagTypeFloat: {
						
						// we get the necessary number of bytes and we transform them into
						// the desired type
						SINGLE_GETFLOAT tagPayload;
						typedef typeof(tagPayload) mtype;
						vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
						if (m_status != good)
							return nullptr;
						m_rehostEndianness<mtype>(tagPayload, buff);
						
						return new Single(tagName, SINGLE_FLOAT(tagPayload));
					}
					
						
					// ====================================================================
					// ====================================================================
					case TagTypeDouble: {
						
						// we get the necessary number of bytes and we transform them into
						// the desired type
						SINGLE_GETDOUBLE tagPayload;
						typedef typeof(tagPayload) mtype;
						vector<byte> buff = m_makeBuffer<mtype>(cursor, end, reassign);
						if (m_status != good)
							return nullptr;
						m_rehostEndianness<mtype>(tagPayload, buff);
						
						return new Single(tagName, SINGLE_DOUBLE(tagPayload));
					}
						
					default:
						m_status = what_the_fuck;
						return nullptr;
				}
			}
		}
	
		// increment the iterator if possible
		void m_secureIncrement(memblock::iterator &mov, memblock::iterator lim, memblock::iterator &reassign) {
			if (mov == lim)
				m_status = null_iterator;
			mov++;
			reassign = mov;
		}
	
		template <typename T>
		vector<byte> m_makeBuffer(memblock::iterator &mov, memblock::iterator lim, memblock::iterator &reassign) {
			
			vector<byte> buff;
			for (int i = 0; i < sizeof(T); i++) {
				buff.push_back(*mov);
				if (i < sizeof(T) - 1) m_secureIncrement(mov, lim, reassign);
			}
			
			return buff;
		}
	
		// assign to a buffer bytes in correct endian-type
		template <typename T>
		void m_rehostEndianness(T &temp, vector<byte> &buff) {
			
			if (HostEndianness().isBig()) {
				for (int j = 0; j < sizeof(T); ++j)
					(reinterpret_cast<char *>(&temp))[j] = buff[j];
			}
			else {
				for (int j = 0; j < sizeof(T); ++j)
					(reinterpret_cast<char *>(&temp))[j] = buff[(sizeof(T) - 1) - j];
			}
		}
};

#endif
