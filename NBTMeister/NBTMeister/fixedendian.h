// *********************************************************
// Modified by Marc-André Brochu AKA Mister Guacamole (2013)
// *********************************************************

// fixedendian.h               Copyright (C) 2010, Real-Time Systems Inc.
//--------------------------------------- All Rights Reserved -----------
//
//  Data Types of Fixed Endianness
//
//  When communicating between different processors using fixed-format
//  binary messages, it's helpful to use a common header file to
//  describe the messages in the protocol.  The problem comes when the
//  processors are of differing endianness.
//
//  The following templates support declaring multi-byte objects whose
//  byte-order in memory is identical on all processors.  If the desired
//  byte order matches the processor's endianness, simple loads and
//  stores are used to access the objects.  On the other hand, if the
//  desired order is opposite the processor's natural byte order, the
//  values are byte swapped when creating the objects and when
//  converting back to normal types.
//
//  Use these templates to define structures for the protocol messages,
//  then simply read and write the fields as usual.  Byte swapping will
//  occur automatically if needed when the fields in the objects are
//  read or written.
//
//-----------------------------------------------------------------------

#ifndef FIXEDENDIAN_H
#define FIXEDENDIAN_H

#include <stddef.h>


// Test the endianness of the processor by "storing" 1 as a multi-byte
// number and "retrieving" its first byte.  Since the compiler knows
// everything about such an object and about its use, the object and the
// test on it will collapse completely at compile-time.
//
// Example:
//
//  if (HostEndianness().isBig())
//    // code for big-endian systems
//  else
//    // code for little-endian systems
union HostEndianness {
	
	int i;
	char c[sizeof(int)];
	
	HostEndianness() : i(1) {}
	
	bool isBig() const { return c[0] == 0; }
	bool isLittle() const { return c[0] != 0; }
};


// Store an object of type 'T' in memory in the byte order indicated by
// 'wantBig' -- big-endian if 'wantBig' is true, else little-endian.
// Provide access to the object via a conversion constructor and a
// conversion operator, swapping bytes on input and output if the
// processor's byte order differs from the desired byte order.
template <typename T, bool wantBig>
class FixedEndian {
	
	public:
	
		// constructors/accessors
		FixedEndian() { }
		FixedEndian(const T& t) : rep(swapbytes(t)) { }
		operator T() const { return swapbytes(rep); }
	
		// ---------------------------------
		// copy semantics
		FixedEndian(const FixedEndian &s) { rep = s.rep; }
		FixedEndian &operator=(const FixedEndian &s) const { rep = s.rep; return *this; }
		// ---------------------------------
	
		// ---------------------------------
		// move semantics
		FixedEndian(FixedEndian &&m) {
			rep = m.rep;
			m.rep = 0;
		}
		
		FixedEndian &operator=(FixedEndian &&m) {
			rep = m.rep;
			m.rep = 0;
			return *this;
		}
		// ---------------------------------
	
		
		FixedEndian &operator=(const T &s) { rep = s; return *this; }
		FixedEndian &operator=(const T &&m) {
			rep = m;
			m = 0;
		}
		
		// get the type of the value
		const std::type_info &type() { return typeid(T); }
	
	private:
		// Object contents, in specified byte order.
		T rep;
		
		// Swap bytes if the processor byte order differs from the specified
		// memory order.
		static T swapbytes(const T& arg) {
			
			// If the processor byte order is the same as the desired byte
			// order, simply return the argument unchanged.
			if (HostEndianness().isBig() == wantBig)
				return arg;
			
			// The processor byte order differs from the specified memory
			// order, so swap the bytes of the argument before returning it.
			else {
				
				T ret;
				char* dst = reinterpret_cast<char*>(&ret);
				const char* src = reinterpret_cast<const char*>(&arg + 1);
				
				for (size_t i = 0; i < sizeof(T); i++)
					*dst++ = *--src;
				
				return ret;
			}
		}
};

// Maintain an object in big-endian order, regardless of the host
// machine's native byte order.
template <typename T>
class BigEndian : public FixedEndian<T, true> {
	
	public:
		BigEndian() { }
		BigEndian(const T& t) : FixedEndian<T, true>(t) {}
};

// Maintain an object in little-endian order, regardless of the host
// machine's native byte order.
template <typename T>
class LittleEndian : public FixedEndian<T, false> {
	
	public:
		LittleEndian() { }
		LittleEndian(const T& t) : FixedEndian<T, false>(t) {}
};

#endif // FIXEDENDIAN_H
