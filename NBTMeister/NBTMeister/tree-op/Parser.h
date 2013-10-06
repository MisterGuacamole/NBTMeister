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
#include "../tags/Tag.h"

using namespace std;

typedef vector<char> memblock; // used for storing binary data
typedef void (*feedback_fct)(double); // function pointer used to send feedback when building the tree

// an enum for the error states of the parser
enum parser_status {
	good,
	range_illegal,
	malformed_stream,
	null_iterator
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
			return build(cursor, end, cursor, feedback);
		}
	
		// this function builds a tree from the data passed.
		// it creates an object on the heap, so don't forget to free it!
		// you need to pass the range in which the parser will do its job
		// About the reassign parameter: its purpose is to set the external iterator the same as the internal iterator while recursion
		Tag *build(memblock::iterator cursor, memblock::iterator end, memblock::iterator &reassign, feedback_fct feedback = nullptr) {
			
			// we make sure the begin is not greater than the end
			if (cursor - end > 0) {
				m_status = range_illegal;
				return nullptr; // return NULL. error checking is your friend
			}
			
			// we parse all of the data, one byte at time
			typedef char byte;
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
				else if (tagType == TagTypeEnd) // we just skip the tag since this is a TagEnd
					return nullptr;
				
				// get the length of the name
				m_secureIncrement(cursor, end, reassign);
				byte _1nameLength = *cursor;
				m_secureIncrement(cursor, end, reassign);
				byte _2nameLength = *cursor;
				
				// if force little endian is on, we make sure data is stored into little-endian form
				#ifdef NBTMEISTER_FORCE_LITTLE_ENDIAN
				BigEndian<uint16_t> tagNameLength((_1nameLength << 8) | _2nameLength);
				#else
				uint16_t nameLength = (_1nameLength << 8) | _2nameLength;
				#endif
				
				if (m_status != good) // an error has happened while fetching some bytes
					return nullptr;
				
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
				// If it is a Single, we setup the tag with its payload and we return it. That way, this function
				// being recursive, the upper level (assuming it is a Compound) will get a beautiful Single Tag.
				// If it is a Compound, we setup the compound and we make a recursive call to this function until we
				// find a TagEnd
				m_secureIncrement(cursor, end, reassign);
				if (m_status != good) // an error has occured
					return nullptr;
				
				if (tagType == TagTypeList || tagType == TagTypeCompound) {
					
					ArrayType atype = (tagType == TagTypeList ? ArrayType::List : ArrayType::Compound);
					Array *root = new Array(tagName, atype);
					while (Tag *ret = build(cursor, end, cursor))
						root->addTag(ret);
					
					return root;
				}
				else if (tagType == TagTypeString) { // we read 2 bytes to get the length, then that number of bytes
					
					byte _1payloadLength = *cursor;
					m_secureIncrement(cursor, end, reassign);
					byte _2payloadLength = *cursor;
					
					// if force little endian is on, we make sure data is stored into little-endian form
					#ifdef NBTMEISTER_FORCE_LITTLE_ENDIAN
					BigEndian<uint16_t> tagPayloadLength((_1payloadLength << 8) | _2payloadLength);
					#else
					uint16_t tagPayloadLength = (_1payloadLength << 8) | _2payloadLength;
					#endif
					
					string tagPayload;
					for (int i = 0; i < tagPayloadLength; i++) {
						
						m_secureIncrement(cursor, end, reassign); // fetch the next byte
						if (m_status != good) // an error has occured
							return nullptr;
						
						// everything is ok, we can get the byte
						tagPayload += *cursor;
					}
					
					m_secureIncrement(cursor, end, reassign); // fetch the next byte
					if (m_status != good) // an error has occured
						return nullptr;
					return new Single(tagName, tagPayload);
				}
			}
			
			return nullptr;
		}
	
		// returns the 'status' of the parser
		parser_status status() { return m_status; }
	
	private:
		parser_status m_status;
	
		// increment the iterator if possible
		void m_secureIncrement(memblock::iterator &mov, memblock::iterator lim, memblock::iterator &reassign) {
			if (mov == lim)
				m_status = null_iterator;
			mov++;
			reassign = mov;
		}
};

#endif
