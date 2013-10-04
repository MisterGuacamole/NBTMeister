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

#ifndef TAG_H
#define TAG_H

#include <string>

using namespace std;

// this enum describes the different 'qualificators' of the tags.
enum TagQualificator {
	QSingle,
	QArray,
	QCompound DEPRECATED,
	QList DEPRECATED
};

/*
 ------------------------------------------------------
 ------------------------------------------------------
 ABOUT THE IMPLEMENTATION
 ------------------------------------------------------
 This is the base class for instanciating tags (singles, compounds or lists).
 It exists to permits the arrays types to contains not only singles but also
 all of the other tags, including themselves.
 
 It also contains an attribute that, when get'd from one of the child types, can
 indicate what is the 'qualificator' of the tag ('single', 'compound' or 'list')
 
 N.B.: This class is abstract, hence you cannot directly instantiate it. Indeed, it
 would not make any sense building a 'Tag' without any further information in the
 context of a NBT tree, since every tag has at least one attribute that can differentiate
 it from the other tags (a name, a payload of a certain type, etc.) A 'Tag' without
 a name and a payload cannot exist as an object in the tree for this reason.
 */
class Tag {
	
	public:
		Tag(TagQualificator qualificator, const string &name) : m_qualif(qualificator), m_name(name) {}
		virtual ~Tag() = 0;
	
		// ----------------------------------------
		// Setters
		// ----------------------------------------
		void setName(const string &name) {
			if (name.empty()) {
				cerr << "[Warning] tried to assign an empty name to a tag" << endl;
				return;
			}
			m_name = name;
		}
	
		// ----------------------------------------
		// Getters
		// ----------------------------------------
		// inherited function that will return the 'TagQualificator' of the tag, i.e. if the
		// tag is a single, a compound or a list
		TagQualificator qualificator() const { return m_qualif; }
		const string &name() const { return m_name; }
	
	protected:
		TagQualificator m_qualif; // represents the qualification of the tag. set at all times
		string m_name;
};

// implements the constructor for this object even if it is virtual, so if the children don't
// reimplement it, there will always be this one to serve. Brave destructor.
inline Tag::~Tag() {}

#endif
