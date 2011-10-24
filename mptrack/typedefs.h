#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#define nullptr		0

//  CountOf macro computes the number of elements in a statically-allocated array.
#if _MSC_VER >= 1400
	#define CountOf(x) _countof(x)
#else
	#define CountOf(x) (sizeof(x)/sizeof(x[0]))
#endif

#define ARRAYELEMCOUNT(x)	CountOf(x)

//Compile time assert. 
#define STATIC_ASSERT(expr)			C_ASSERT(expr)
#define static_assert(expr, msg)	C_ASSERT(expr)


typedef __int8 int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64;

const int8 int8_min	    = -127-1;
const int16 int16_min   = -32767-1;
const int32 int32_min   = -2147483647-1;
const int64 int64_min   = -9223372036854775807-1;

const int8 int8_max     = 127;
const int16 int16_max   = 32767;
const int32 int32_max   = 2147483647;
const int64 int64_max   = 9223372036854775807;

const uint8_t uint8_max   = 255;
const uint16_t uint16_max = 65535;
const uint32_t uint32_max = 4294967295;
const uint64 uint64_max = 18446744073709551615;

typedef float float32;


#if !_HAS_TR1
	namespace std
	{ 
		namespace tr1
		{
			template <class T> struct has_trivial_assign {static const bool value = false;};

			#define SPECIALIZE_TRIVIAL_ASSIGN(type) template <> struct has_trivial_assign<type> {static const bool value = true;}
			SPECIALIZE_TRIVIAL_ASSIGN(int8);
			SPECIALIZE_TRIVIAL_ASSIGN(uint8_t);
			SPECIALIZE_TRIVIAL_ASSIGN(int16);
			SPECIALIZE_TRIVIAL_ASSIGN(uint16_t);
			SPECIALIZE_TRIVIAL_ASSIGN(int32);
			SPECIALIZE_TRIVIAL_ASSIGN(uint32_t);
			SPECIALIZE_TRIVIAL_ASSIGN(int64);
			SPECIALIZE_TRIVIAL_ASSIGN(uint64);
			#undef SPECIALIZE_TRIVIAL_ASSIGN
		};
	};
#endif


#endif
