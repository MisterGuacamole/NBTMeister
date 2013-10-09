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

#include <iostream>
#include "tags/Single.h"
#include "tags/Array.h"
#include "tree-op/Parser.h"
#include "libs/zlib-contrib/zfstream.h"

using namespace std;

int main(int argc, const char * argv[]) {
	
	cout << "Test zlib...\nDecompressing file..." << endl;
	
	gzofstream outf;
	gzifstream inf;
	
	std::cout << "\nReading 'test.nbt' (buffered) produces:\n";
	inf.open("../../NBTMeister/tests/bigtest.nbt", ios::binary);
	if (!inf.is_open()) {
		cout << "Cannot open file" << endl;
		return 0;
	}
	
	const unsigned int chunkSize = 2048;
	
	memblock collectedData(chunkSize);
	inf.read(&collectedData[0], chunkSize);
	inf.close();
	
	for (int i = 0; i < chunkSize; i++) {
		cout << (unsigned int)collectedData[i];
	}
	cout << "\n***\n\n" << endl;
	
	// ---------------------------------
	cout << "Building tree..." << flush;
	Parser parser;
	Tag *tree = parser.build(collectedData.begin(), collectedData.end());
	
	cout << "\n" << endl;
	
	if (parser.status() != parser_status::good)
		cerr << "\n[Error] parser error 0x" << parser.status() << endl;
	
	static_cast<Array *>(tree)->print();
	delete tree;
	
    return 0;
}

