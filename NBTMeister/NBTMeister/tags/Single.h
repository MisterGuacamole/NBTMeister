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

#ifndef SINGLE_H
#define SINGLE_H

#include <string>
#include <vector>
#include <utility>
#include "config.h"
#include "fixedendian.h"
#include "Tag.h"
#include "TagTypes.h"
#include "../libs/ttl/var/variant.hpp"

using namespace std;
namespace var = ttl::var;

// All the possible payload types that a 'single' tag can hold
// see the config.h file to change this
#ifdef NBTMEISTER_FORCE_LITTLE_ENDIAN
typedef var::variant<
	LittleEndian<int8_t>,			// 0- TagTypeByte
	LittleEndian<int16_t>,			// 1- TagTypeShort
	LittleEndian<int32_t>,			// 2- TagTypeInt
	LittleEndian<int64_t>,			// 3- TagTypeLong
	LittleEndian<float>,			// 4- TagTypeFloat
	LittleEndian<double>,			// 5- TagTypeDouble
	vector<LittleEndian<int8_t>>,	// 6- TagTypeByteArray
	vector<LittleEndian<int32_t>>,	// 7- TagTypeIntArray
	string							// 8- TagTypeString
> payload_type;

// some defines to help code lisibility & versatility
#define SINGLE_BYTE(v) LittleEndian<int8_t>(v)
#define SINGLE_SHORT(v) LittleEndian<int16_t>(v)
#define SINGLE_INT(v) LittleEndian<int32_t>(v)
#define SINGLE_LONG(v) LittleEndian<int64_t>(v)
#define SINGLE_FLOAT(v) LittleEndian<float>(v)
#define SINGLE_DOUBLE(v) LittleEndian<double>(v)

#else
typedef var::variant<
	int8_t,				// 0- TagTypeByte
	int16_t,			// 1- TagTypeShort
	int32_t,			// 2- TagTypeInt
	int64_t,			// 3- TagTypeLong
	float,				// 4- TagTypeFloat
	double,				// 5- TagTypeDouble
	vector<int8_t>,		// 6- TagTypeByteArray
	vector<int32_t>,	// 7- TagTypeIntArray
	string				// 8- TagTypeString
> payload_type;

#define SINGLE_BYTE(v) static_cast<int8_t>(v)
#define SINGLE_SHORT(v) static_cast<int16_t>(v)
#define SINGLE_INT(v) static_cast<int32_t>(v)
#define SINGLE_LONG(v) static_cast<int64_t>(v)
#define SINGLE_FLOAT(v) static_cast<float>(v)
#define SINGLE_DOUBLE(v) static_cast<double>(v)

#endif


/*
 ------------------------------------------------------
 ------------------------------------------------------
 ABOUT SINGLE TAGS ('SINGLES') - FROM NBT.txt
 ------------------------------------------------------
 A Named Tag has the following format:
 
 byte tagType
 string name
 [payload]
 
 The tagType is a single byte defining the contents of the payload of the tag.
 
 The name is a descriptive name, and can be anything (eg "cat", "banana", "Hello World!"). It has nothing to do with the tagType.
 The purpose for this name is to name tags so parsing is easier and can be made to only look for certain recognized tag names.
 Exception: If tagType is TAG_End, the name is skipped and assumed to be "".
 
 The [payload] varies by tagType.
 
 Note that ONLY Named Tags carry the name and tagType data. Explicitly identified Tags (such as TAG_String above) only contains the payload.
 
 ------------------------------------------------------
 ------------------------------------------------------
 ABOUT THE IMPLEMENTATION
 ------------------------------------------------------
 *************
 Introduction:
 There can be two general types of tags : those who shall only contain one value, and those who shall contain multiple values (arrays).
 This class is used to represent a single tag in the Tag Tree. It is a 'single', and contrary to 'arrays', it can only contain one value.
 This fact simplifies the implementation of a NBT Tree by limiting our classes to the number of two:
 	
 	1) Singles (The 'Single' object)
 	2) Arrays (The 'Array' object)
 
 **************
 Notes & Usage:
 We use the Single object by specifing a name (obligatory non-void) and a payload (may be void when constructing).
 
 We access the payload using var::get<[Type]>([TagSingleObject].payload()).
 Name and single-tag type can be accessed and reassigned via their own getters and setters.
 
 THE FOLLOWING ONLY APPLIES IF 'NBTMEISTER_FORCE_LITTLE_ENDIAN' IS NOT DEFINED!
 IF IT IS DEFINED, THE USAGE OF 'FIXEDENDIAN' OBJECTS WILL VOID THE FOLLOWING CLAIMS.
 Be sure to correctly cast the value before sending it into this object's constructor, as every non-casted left-op...
 	- integral will be assumed to be of type 'int';
 	- float will be assumed to be of type 'double'.
 
 This may lead to greater problems when trying to set the payload to a greater value than the 'int' capacity.
 In other words, if i > sizeof(int) and i is not correctly casted to type 'long', the compilator may yeld an error.
 Incorrect or inexistant casting may also lead to problems when accessing the data, obviously.
 */
class Single : public Tag {
	
	//************
	public:
		Single(const string &name, const payload_type &val = payload_type()) : Tag(TagQualificator::QSingle, name),
		 m_payload(val) {
			
			 // we lock the tag by setting m_typeLock to 'which'. It is then
			 // not possible to assign a value that is not of the type of the tag
			 // to the tag after this.
			 m_typeLock = val.which();
		}
	
	
		// ----------------------------------------
		// Setters
		// ----------------------------------------
		void setPayload(const payload_type &payload) {
			
			// we shall only assign a value that corresponds to the tag's type.
			// if the values' type doesn't correspond, we need to throw an error
			if (m_typeLock == payload.which())
				m_payload = payload;
			else
				cerr << "[Error] locked tag cannot accept value type in variant" << endl;
		}
	
	
		// ----------------------------------------
		// Getters
		// ----------------------------------------
		const payload_type &payload() const { return m_payload; }
		TagType tagType() const {
			
			// TODO: This function is hard-coded, disgusting. Maybe I should use a visitor or something
			
			// 1-6 are correctly aligned in the variant.
			// By that I mean that index 1 in the variant is
			// equal to index 1 in the enum.
			if (m_typeLock < 7)
				return static_cast<TagType>(m_typeLock + 1);
			
			switch (m_typeLock) {
				case 7:
					return TagTypeIntArray;
				case 8:
					return TagTypeString;
					
				default:
					cerr << "[Error] tag type is not of a valid type, cannot return it" << endl;
					break;
			}
			return TagTypeEnd; // should not happen, may want to throw an exception
		}
	
	
	//************
	protected:
		// This is used to 'fix' the tag, blocking the user from using it
		// for multiple value types.
		unsigned int m_typeLock;
	
	//************
	private:
		// the payload specified when creating the single-tag.
		// When the single-tag is of type ByteArray or IntArray,
		// the payload is not the lenght but actually the array
		payload_type m_payload;
};

#endif
