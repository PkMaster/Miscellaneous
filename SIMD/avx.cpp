// ConsoleApplication2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <Windows.h>

#include <mmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>

#include <stdlib.h>
#include <assert.h>

#include <fstream>

///////////////////////////

void calculateUsingMmxInt(char* data, unsigned size)
{
	assert(size % 8 == 0);

	__m64 step = _mm_set_pi8(10, 10, 10, 10, 10, 10, 10, 10);
	__m64* dst = reinterpret_cast<__m64*>(data);
	for (unsigned i = 0; i < size; i += 8)
	{
		auto sum = _mm_adds_pi8(step, *dst);
		*dst++ = sum;
	}

	_mm_empty();
}

void calculateUsingSseInt(char* data, unsigned size)
{
	assert(size % 16 == 0);

	__m128i step = _mm_set_epi8(10, 10, 10, 10, 10, 10, 10, 10,
								10, 10, 10, 10, 10, 10, 10, 10);
	__m128i* dst = reinterpret_cast<__m128i*>(data);
	for (unsigned i = 0; i < size; i += 16)
	{
		auto sum = _mm_add_epi8(step, *dst);
		*dst++ = sum;
	}

	//	no need to clear flags like mmx because SSE and FPU can be used at the same time.
}

void calculateUsingAveInt(char* data, unsigned size)
{
	assert(size % 32 == 0);

	__m256i step = _mm256_set_epi8(10, 10, 10, 10, 10, 10, 10, 10,
								   10, 10, 10, 10, 10, 10, 10, 10,
								   10, 10, 10, 10, 10, 10, 10, 10,
								   10, 10, 10, 10, 10, 10, 10, 10);
	__m256i* dst = reinterpret_cast<__m256i*>(data);
	for (unsigned i = 0; i < size; i += 32)
	{
		auto sum = _mm256_add_epi8(step, *dst);
		*dst++ = sum;
	}
}

////////////////////////

void calculateUsingAsmFloat(float* data, unsigned count)
{
	auto singleFloatBytes = sizeof(float);
	auto step = 10.0;
	__asm
	{
			push ecx
			push edx

			mov edx, data
			mov ecx, count
			fld	step	//	fld only accept FPU or Memory

		calcLoop:
			fld [edx]
			fadd st(0), st(1)
			fstp [edx]
			add edx, singleFloatBytes
			dec ecx
			jnz calcLoop

			pop edx
			pop ecx
	}
}

void calculateUsingSseFloat(float* data, unsigned count)
{
	assert(count % 4 == 0);
	assert(sizeof(float) == 4);

	__m128 step = _mm_set_ps(10.0, 10.0, 10.0, 10.0);
	__m128* dst = reinterpret_cast<__m128*>(data);
	for (unsigned i = 0; i < count; i += 4)
	{
		__m128 sum = _mm_add_ps(step, *dst);
		*dst++ = sum;
	}
}

void calculateUsingAveFloat(float* data, unsigned count)
{
	assert(count % 8 == 0);
	assert(sizeof(float) == 4);

	__m256 step = _mm256_set_ps(10.0, 10.0, 10.0, 10.0,
								10.0, 10.0, 10.0, 10.0);
	__m256* dst = reinterpret_cast<__m256*>(data);
	for (unsigned i = 0; i < count; i += 8)
	{
		__m256 sum = _mm256_add_ps(*dst, step);
		*dst++ = sum;
	}
}


//////////////////////////

LARGE_INTEGER g_counterBegin;
LARGE_INTEGER g_counterEnd;

void beginBenchmark()
{
	if (QueryPerformanceCounter(&g_counterBegin) == 0) {
		fprintf(stderr, "benchmark error:QueryPerformanceCounter");
		exit(-3);
	}
}

void endBenchmark()
{
	if (QueryPerformanceCounter(&g_counterEnd) == 0) {
		fprintf(stderr, "benchmark error:QueryPerformanceCounter");
		exit(-4);
	}
}

void printBenchmarkTime()
{
	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency) == 0) {
		fprintf(stderr, "benchmark error:QueryPerformanceFrequency");
		exit(-5);
	}

	fprintf(stdout, "Benchmark time: %f s\n", (g_counterEnd.QuadPart - g_counterBegin.QuadPart) / (float)frequency.QuadPart);
}

/////////////////////////

void saveToFile(const char* filename, char* data, unsigned size)
{
	std::ofstream outfile(filename);
	outfile.write(data, size);
}

/////////////////////////

bool supportsAve()
{
	__asm
	{
			push ecx

			mov eax, 0
			cpuid
			cmp ecx, 1
			jb notSupported	//	check if supports EAX=1 when using CPUID

			mov eax, 1
			cpuid
			and ecx, 0x18000000	//	clear non-related bits
			cmp ecx, 0x18000000	//	check OSXSAVE and avx
			jne notSupported

			mov ecx, 0
			XGETBV				//	get XCR0 register value
			and eax, 0x6
			cmp eax, 0x6		//	check XMM and YMM state
			jne notSupported

			mov eax, 1
			jmp done

		notSupported:
			mov eax, 0

		done:
			pop ecx
	}
}

bool supportAvx2()
{
	__asm
	{
			push ecx

			mov eax, 0
			cpuid
			cmp ecx, 7
			jb notSupported	//	check if supports EAX=7 when using CPUID

			mov eax, 1
			cpuid
			and ecx, 0x18000000	//	clear non-related bits
			cmp ecx, 0x18000000	//	check OSXSAVE and avx
			jne notSupported

			mov eax, 7
			cpuid
			and ecx, 0x20
			cmp ecx, 0x20 //	check avx2
			jne notSupported

			mov ecx, 0
			XGETBV				//	get XCR0 register value
			and eax, 0x6
			cmp eax, 0x6		//	check XMM and YMM state
			jne notSupported

			mov eax, 1
			jmp done

		notSupported :
			mov eax, 0

		done :
			pop ecx
	}
}

/////////////////////////

int main()
{
	constexpr unsigned intDataSizeInBytes = 10000000;

	auto dataInt = static_cast<char*>(calloc(intDataSizeInBytes, 1));
	if (dataInt == 0) {
		fprintf(stderr, "allocate 10000000-byte memory failed!");
		return -1;
	}

	beginBenchmark();
	calculateUsingMmxInt(dataInt, intDataSizeInBytes);
	endBenchmark();
	printBenchmarkTime();

	///////

	beginBenchmark();
	calculateUsingSseInt(dataInt, intDataSizeInBytes);
	endBenchmark();
	printBenchmarkTime();

	//////

	if (supportsAve() && supportAvx2()) {
		beginBenchmark();
		calculateUsingAveInt(dataInt, intDataSizeInBytes);
		endBenchmark();
		printBenchmarkTime();
	} else {
		fprintf(stderr, "-------Avx or Avx2 not supported!\n");
	}
	

	///////////////////////////////////

	constexpr unsigned floatCount = 10000000;
	constexpr unsigned floatDataSizeInBytes = floatCount * sizeof(float);
	auto dataFloat = static_cast<float*>(malloc(floatDataSizeInBytes));
	if (dataFloat == 0) {
		fprintf(stderr, "allocate 10000000-float memory failed!");
		return -1;
	}

	//	init float to 0.0
	float* tmp = reinterpret_cast<float*>(dataFloat);
	for (unsigned i = 0; i < floatCount; ++i)
		*tmp++ = 0.0;

	beginBenchmark();
	calculateUsingAsmFloat(dataFloat, floatCount);
	endBenchmark();
	printBenchmarkTime();

	///////

	beginBenchmark();
	calculateUsingSseFloat(dataFloat, floatCount);
	endBenchmark();
	printBenchmarkTime();

	if (supportsAve()) {
		beginBenchmark();
		calculateUsingAveFloat(dataFloat, floatCount);
		endBenchmark();
		printBenchmarkTime();
	} else {
		fprintf(stderr, "--------Avx not supported!\n");
	}

    return 0;
}

