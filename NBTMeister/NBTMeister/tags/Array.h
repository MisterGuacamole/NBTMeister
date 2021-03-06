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

#ifndef ARRAY_H
#define ARRAY_H

#include <vector>
#include <algorithm>
#include "Tag.h"

enum ArrayType {
	List,
	Compound
};

/*
 ------------------------------------------------------
 ------------------------------------------------------
 ABOUT ARRAY TAGS - FROM NBT.txt
 ------------------------------------------------------
 TYPE: 9  NAME: TAG_List
 Payload: TAG_Byte tagId
 		  TAG_Int length
 		  A sequential list of Tags (not Named Tags), of type <typeId>.
 		  The length of this array is <length> Tags
 Notes:   All tags share the same type.
 
 TYPE: 10 NAME: TAG_Compound
 Payload: A sequential list of Named Tags. This array keeps going until a TAG_End is found.
 		  TAG_End end
 Notes:	  If there's a nested TAG_Compound within this tag, that one will also have a TAG_End, so simply reading until the next TAG_End will not work.
		  The names of the named tags have to be unique within each TAG_Compound
 		  The order of the tags is not guaranteed.
 
 ------------------------------------------------------
 ------------------------------------------------------
 ABOUT THE IMPLEMENTATION
 ------------------------------------------------------
 *************
 Introduction: see the Single.h file for the complete introduction.
 
 **************
 Notes & Usage:
 'Array' is used for representing both Compounds and Lists in the NBT tree.
 */
class Array : public Tag, private vector<Tag *> {
	
	public:
		Array(const string &name, ArrayType atype, TagType listType = TagTypeInvalid) :
		Tag(TagQualificator::QArray, name), m_arrayType(atype), m_listType(listType) {
			m_currPtr = begin();
		}
		
		~Array() {
			for (Tag *t : *this) // the delete operation may trigger other delete operations in inner tags
				delete t;
		}
		
	
		// ----------------------------------------
		// Public functions
		// ----------------------------------------
		void addTag(Tag *t) {
			
			// we need to make sure this is not called in a 'nextTag' type of loop
			m_assertPtr(t);
			push_back(t);
			m_currPtr = begin(); // m_currPtr is invalidated after a push_back operation, so we reset it here
		}
	
		// remove a specific tag from the array. If the operation fails to
		// complete, returns false, otherwise returns true
		bool removeTag(Tag *t) {
			
			// we need to make sure this is not called in a 'nextTag' type of loop
			m_assertPtr(t);
			
			// this is a simple algorithm method that will find and then erase
			// the tag passed
			iterator it = find(begin(), end(), t);
			if (it == end())
				return false; // if we didn't find the tag in the array
			
			erase(it);
			m_currPtr = begin(); // m_currPtr is invalidated after an erase operation, so we reset it here
			return true;
		}
	
		// get a tag by its name
		Tag *tag(const string &name) {
			
			for (Tag *t : *this) {
				if (t->name() == name)
					return t;
			}
			return nullptr; // we didn't find the tag
		}
	
		// returns the next tag and advance the pointer to the following tag
		Tag *nextTag() {
			
			if (m_currPtr != end()) {
				Tag *currentTag = *m_currPtr;
				m_currPtr++;
				return currentTag;
			}
			return nullptr; // we are at the end of the array
		}
	
		// seeks (reposition) the pointer
		void seek(iterator pos) {
			m_currPtr = pos;
		}
	
		// debug method. will print its hierarchy
		void print(int lvl = 0) {
			
			string myType;
			switch (arrayType()) {
				case List:
					myType = "List";
					break;
				default: myType = "Compound";
			}
			
			cout << myType << "(\"" << name() << "\"): " << size() << " entries" << flush;
			if (arrayType() == List) {
				cout << " of type ";
				switch (m_listType) {
					case TagTypeByte: cout << "Byte" << flush; break;
					case TagTypeShort: cout << "Short" << flush; break;
					case TagTypeInt: cout << "Int" << flush; break;
					case TagTypeLong: cout << "Long" << flush; break;
					case TagTypeFloat: cout << "Float" << flush; break;
					case TagTypeDouble: cout << "Double" << flush; break;
					case TagTypeByteArray: cout << "ByteArray" << flush; break;
					case TagTypeIntArray: cout << "IntArray" << flush; break;
					case TagTypeString: cout << "String" << flush; break;
					case TagTypeList: cout << "List" << flush; break;
					case TagTypeCompound: cout << "Compound" << flush; break;
					default: cout << "Invalid" << flush; break;
				}
			}
			cout << endl;
			
			for (int i = 0; i < lvl; i++)
				cout << "\t" << flush;
			cout << "{\n" << flush;
			
			for (Tag *t : *this) {
				
				for (int i = 0; i <= lvl; i++)
					cout << "\t" << flush;
				
				if (t->qualificator() == TagQualificator::QSingle) {
					switch (static_cast<Single *>(t)->tagType()) {
						case TagTypeByte:
							cout << "Byte(\"" << t->name() << "\"): " << (int)static_cast<Single *>(t)->toByte() << endl;
							break;
						case TagTypeShort:
							cout << "Short(\"" << t->name() << "\"): " << static_cast<Single *>(t)->toShort() << endl;
							break;
						case TagTypeInt:
							cout << "Int(\"" << t->name() << "\"): " << static_cast<Single *>(t)->toInt() << endl;
							break;
						case TagTypeLong:
							cout << "Long(\"" << t->name() << "\"): " << static_cast<Single *>(t)->toLong() << endl;
							break;
						case TagTypeFloat:
							cout << "Float(\"" << t->name() << "\"): " << static_cast<Single *>(t)->toFloat() << endl;
							break;
						case TagTypeDouble:
							cout << "Double(\"" << t->name() << "\"): " << static_cast<Single *>(t)->toDouble() << endl;
							break;
						case TagTypeByteArray:
							cout << "ByteArray(\"" << t->name() << "\"): " << flush;
							for (int i = 0; i < static_cast<Single *>(t)->toByteArray().size() - 1; i++)
								cout << (int)static_cast<Single *>(t)->toByteArray()[i] << ", " << flush;
							cout << static_cast<Single *>(t)->toByteArray()[static_cast<Single *>(t)->toByteArray().size() - 1] << endl;
							break;
						case TagTypeIntArray:
							cout << "IntArray(\"" << t->name() << "\"): " << flush;
							for (int i = 0; i < static_cast<Single *>(t)->toIntArray().size() - 1; i++)
								cout << static_cast<Single *>(t)->toIntArray()[i] << ", " << flush;
							cout << static_cast<Single *>(t)->toIntArray()[static_cast<Single *>(t)->toIntArray().size() - 1] << endl;
							break;
						case TagTypeString:
							cout << "String(\"" << t->name() << "\"): " << static_cast<Single *>(t)->toString() << endl;
							break;
						default: break;
					}
				}
				else static_cast<Array *>(t)->print(lvl + 1);
			}
			
			for (int i = 0; i < lvl; i++)
				cout << "\t" << flush;
			
			cout << "}" << endl;
		}
	
	
		// ----------------------------------------
		// Simple getters
		// ----------------------------------------
		size_t size() { return vector<Tag *>::size(); }
		Tag *tag(size_t index) { return at(index); } // get a tag by its index (only useful for looping purposes since order in the array is not guaranteed)
		ArrayType arrayType() { return m_arrayType; }
		TagType listType() { return m_listType; }
	
	
	private:
		ArrayType m_arrayType;
		TagType m_listType; // if the array is a list, the type of the list
		iterator m_currPtr; // used in next() function, this pointer is used to act like a seek pointer
	
		// this function will check if the m_currPtr pointer has been modified
		void m_assertPtr(Tag *t = nullptr) {
			if (m_currPtr != begin()) {
				cerr << "[Fatal] cannot modify an array while iterating through it" << endl;
				delete t; // avoid mem leak
				throw 0x0; // TODO: Fix exeptions
			}
		}
};

#endif
