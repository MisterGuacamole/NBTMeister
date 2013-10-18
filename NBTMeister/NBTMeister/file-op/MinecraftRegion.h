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

#ifndef MINECRAFTREGION_H
#define MINECRAFTREGION_H

#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <zlib.h>
#include "Parser.h"
#include "../config.h"
#include "../libs/zlib-contrib/zfstream.h"

using namespace std;

#ifdef NBTMEISTER_USE_MINECRAFT_NAMESPACE
namespace Minecraft {
#endif // NBTMEISTER_USE_MINECRAFT_NAMESPACE


class Region {
	
	public:
		Region() : m_rawData(), m_chunks(), m_good(false) {}
		Region(const string &path) : m_rawData(), m_chunks(), m_good(false) { open(path); }
	
		// open the file and read all of the bytes from it
		void open(const string &path) {
			
			// we start by opening the file
			ifstream infile(path, ios::binary);
			m_good = infile.good();
			if (!m_good)
				return;
			
			// the file opened correctly, we fill the raw data table
			// with the contents of the file
			infile.seekg(0, infile.end);
			size_t length = infile.tellg(); // we get the length of the file
			
			m_rawData.resize(length); // we resize the vector so it can contain the file bytes
			infile.seekg(0, infile.beg);
			infile.read(&m_rawData[0], length); // we fill the vector with the data
		}
	
		void mapChunks() {
			// we don't want to process the file if it has not been opened correctly
			if (!m_good)
				return;
			m_process(); // we read the compressed data
		}
	
		bool good() { return m_good; }
	
	private:
		bool m_good;
		memblock m_rawData;
		vector<memblock> m_chunks;
	
		// reads the header and the compressed chunk data, then decompress it
		void m_process() {
			
			const int locationTableSize = 4096;
			const int sectorSizeInBytes = 4096;
			
			if (m_rawData.size() < locationTableSize * 2) { // x2 for the timestamp table
				m_good = false;
				return;
			}
			
			// the array long enough to not need to check out of bounds
			int index = 0;
			while(index < locationTableSize) {
				
				// we get the first 3 bytes of the table : the offset
				vector<int8_t> buff;
				buff.push_back(m_rawData[index]); index++; // 1
				buff.push_back(m_rawData[index]); index++; // 2
				buff.push_back(m_rawData[index]); index++; // 3
				
				// we make the endianess match the machine's
				// warning ugly-ass code below for 3 lines
				SINGLE_GETINT offset = 0;
				// this ugly code below switches the endianness of the int if needed
				if (HostEndianness().isBig()) for (int j = 0; j < 3; ++j) (reinterpret_cast<char *>(&offset))[j] = buff[j]; // yes this is ugly-ass code speaking,
				else for (int j = 0; j < 3; ++j) (reinterpret_cast<char *>(&offset))[j] = buff[(3 - 1) - j];				// what can I do for you today?
				
				
				// ------
				// we get the next byte : the length
				int8_t length = m_rawData[index]; // index is already incremented from the index++ above
				index++;
				
//				cout << "Building chunks... (" << setprecision(1) << fixed << (((float)index / 4) / 1024) * 100 << "% completed)" << endl;
				
				// if both length and offset are 0, the chunk are not yet loaded into the file so we skip this loop
				if ((length | offset) == 0) {
					m_chunks.push_back(memblock(0)); // put an empty chunk into the array for future reference
					continue;
				}
				
				
				// ------
				// we need to get the exact remaining length of compressed data (4 bytes) then the type of compression (1 byte)
				buff.clear();
				for (int8_t i = 0; i < 4; i++)
					buff.push_back(m_rawData[(offset * sectorSizeInBytes) + i]);
				
				// we make the endianess match the machine's
				// warning ugly-ass code below for 3 lines
				SINGLE_GETINT remainingLength = 0;
				// this ugly code below switches the endianness of the int if needed
				if (HostEndianness().isBig()) for (int j = 0; j < 4; ++j) (reinterpret_cast<char *>(&remainingLength))[j] = buff[j];
				else for (int j = 0; j < 4; ++j) (reinterpret_cast<char *>(&remainingLength))[j] = buff[(4 - 1) - j];
				
				// we skip the next byte (compression type)
				// the gzip compression type is never used, but it would be good to support it IF one day it is used
				// TODO: Support GZip
				
				
				// ------
				// we can read the chunk data here
				// we get all the bytes in the range [offset, offset + length]
				memblock compressedBytes;
				for (unsigned long int currentByte = 0; currentByte < remainingLength; currentByte++)
					compressedBytes.push_back(m_rawData[(offset * sectorSizeInBytes) + currentByte + 5]); // the "+ 5" is to skip the chunk header (5 bytes wide)
				
				// we decompress the chunk data
				uLongf outputSize = 65536;
				memblock decompressedBytes(outputSize);
				while (m_decompressChunk(compressedBytes, decompressedBytes) == Z_BUF_ERROR) {
					outputSize += 4096;
					decompressedBytes.resize(outputSize);
				}
				
				m_chunks.push_back(decompressedBytes);
			}
			
			Parser parser;
			
//			ofstream outbitch("outbitch", ios::binary);
//			for (int fj = 0; fj < m_chunks[8].size(); fj++)
//				outbitch.put(m_chunks[8][fj]);
			
			Array *bestial = (Array *)parser.build(m_chunks[8].begin(), m_chunks[8].end());
			if (bestial)
				bestial->print();
			else cout << "lolilel" << endl;
		}
	
		int m_decompressChunk(memblock &compressed, memblock &output) {
			const Bytef *input = reinterpret_cast<const Bytef *>(compressed.data());
			uLongf inlen = output.size();
			return uncompress((Bytef *)(&output.front()), &inlen, input, compressed.size());
		}
};
	
#ifdef NBTMEISTER_USE_MINECRAFT_NAMESPACE
};
#endif // NBTMEISTER_USE_MINECRAFT_NAMESPACE
#endif // MINECRAFTREGION_H
