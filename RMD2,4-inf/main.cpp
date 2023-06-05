#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <iostream>
#include <windows.h>
#include <stdint.h>
using namespace std;

double m_time;

// manage data
void readCodes(string path, uint32_t*& codes, uint32_t& codesN)
{
	FILE* in = fopen(path.c_str(), "rb");

	fseek(in, 0, SEEK_END);
	codesN = ftell(in) / sizeof(uint32_t);
	fseek(in, 0, SEEK_SET);

	codes = new uint32_t[codesN];
	fread(codes, sizeof(uint32_t), codesN, in);

	fclose(in);
}

void writeCodes(string path, uint32_t* codes, uint32_t codesN)
{
	FILE* out = fopen(path.c_str(), "wb");

	fwrite(codes, sizeof(uint32_t), codesN, out);

	fclose(out);
}

void readFile(string path, uint8_t*& file, uint32_t& fileN)
{
	FILE* in = fopen(path.c_str(), "rb");

	fseek(in, 0, SEEK_END);
	fileN = ftell(in);
	fseek(in, 0, SEEK_SET);

	file = new uint8_t[fileN];
	fread(file, sizeof(uint8_t), fileN, in);

	fclose(in);
}

void writeFile(string path, uint8_t* file, uint32_t fileN)
{
	FILE* out = fopen(path.c_str(), "wb");

	fwrite(file, sizeof(uint8_t), fileN, out);

	fclose(out);
}

// encode text
uint32_t* rmd;

void genRMD1(int32_t maxCode)
{
	int32_t nl[45] = { 0 };
	int32_t k[20] = { 0,1,3,30 };
	int32_t kmax = 4, Lmin = 3;

	rmd = new uint32_t[maxCode + 2];

	int32_t L, n, L1_long = -1, L1_long_prev = -1, t = Lmin - 1;
	uint32_t seq = 0, s = 0;

	for (L = Lmin, n = 0; n <= maxCode; L++)
	{
		nl[L] = n;
		for (int32_t i = 0; L - k[i] > Lmin && i < kmax; i++)
		{
			if (i == kmax - 1 && L1_long == -1)
			{
				L1_long = n;
			}

			for (int32_t j = nl[L - k[i] - 1]; j < nl[L - k[i]] && n <= maxCode; j++, n++)
			{
				seq = 0xFFFFFFF >> (28 - k[i]);
				rmd[n] = (rmd[j] << (k[i] + 1)) | seq;
			}
		}
		if (L1_long_prev > -1)
		{
			for (int32_t j = L1_long_prev; j < nl[L] && n <= maxCode; j++, n++)
			{
				rmd[n] = (rmd[j] << 1) | 1;
			}
		}
		if (t < kmax && L - 1 != k[t])
		{
			rmd[n] = (1 << (L - 1)) - 1;
			n++;
		}
		else
		{
			t++;
		}
		L1_long_prev = L1_long;
		L1_long = -1;
	}
	seq = 1;
}

void encodeCode(uint32_t x, int32_t& curByte, int32_t& curBit, uint8_t* file)
{
	int32_t k(0), p;
	uint32_t j, t = 32;

	for (j = 1 << t - 1, p = t - 1; !(x & j) && j; j >>= 1, p--);

	for (j <<= 1; j > 0; j >>= 1)
	{
		if (j & x)
		{
			file[curByte] |= (1 << curBit);
		}

		curBit = curBit == 0 ? 7 : curBit - 1;

		if (curBit == 7)
		{
			curByte++;
		}
	}
}

void encodeCodes1(uint32_t* codes, uint32_t codesN, uint8_t*& file, uint32_t& fileN)
{
	file = new uint8_t[sizeof(uint32_t) * codesN];
	memset(file, 0, sizeof(uint32_t) * codesN);

	*(uint32_t*)file = codesN;

	uint32_t maxCode = 0;
	for (uint32_t i = 0; i < codesN; i++)
	{
		maxCode = maxCode < codes[i] ? codes[i] : maxCode;
	}

	*(uint32_t*)(file + 4) = maxCode;

	genRMD1(maxCode);

	int32_t i, curByte = 8, curBit = 7;

	for (i = 0; i < codesN; i++)
	{
		encodeCode(rmd[codes[i]], curByte, curBit, file);
	}

	encodeCode(rmd[0], curByte, curBit, file); // dummy

	fileN = curBit == 7 ? curByte : curByte + 1;

	delete[] rmd;
}

// decode text
#define measure_start() LARGE_INTEGER start, finish, freq; \
	QueryPerformanceFrequency(&freq); \
	QueryPerformanceCounter(&start);

#define measure_end() QueryPerformanceCounter(&finish); \
					m_time = ((finish.QuadPart - start.QuadPart) / (double)freq.QuadPart);

struct ai_state
{
	int32_t st, L, p, res;
};

struct tabsrmd1
{
	int32_t p1, p2, p3, p4, n;
	tabsrmd1* al;
};

int32_t state_max = 3;
int32_t nl[45];
int32_t nlk[45][45];

#define tableSize (1 << 16)

uint8_t ptr_rmd[tableSize], rmd_n[tableSize];
uint64_t rmd_numbers[tableSize], extra_rmd_numbers[tableSize];
uint32_t rmd_transfer[tableSize], rmd_flag[tableSize];

void pack_new(int32_t x, tabsrmd1 v, ai_state s)
{
	ptr_rmd[x] = state_max * s.L + s.st;

	if (v.p2 != -1)
	{
		rmd_flag[x] = 0;
		rmd_n[x] = 1;
		rmd_numbers[x] = v.p1 + ((uint64_t)v.p2 << 32);
		extra_rmd_numbers[x] = 0;
		rmd_transfer[x] = v.p2;

		if (v.p3 > -1)
		{
			rmd_n[x] = 2;
			rmd_transfer[x] = v.p3;
			extra_rmd_numbers[x] = v.p3;

			if (v.p4 > -1)
			{
				rmd_n[x] = 3;
				extra_rmd_numbers[x] += ((uint64_t)v.p4 << 32);
				rmd_transfer[x] = v.p4;
			}
		}
	}
	else
	{
		rmd_n[x] = 0;
		rmd_flag[x] = 0xffffff;
		rmd_transfer[x] = v.p1;
		rmd_numbers[x] = v.p1;
	}
}

void genRMD1_nl_nlk(int32_t maxCode)
{
	int32_t k[20] = { 0,1,3,30 };
	int32_t kmax = 4, Lmin = 3;

	int32_t L, n, L1_long = -1, L1_long_prev = -1, t = Lmin - 1;
	uint32_t s = 0;

	for (L = Lmin, n = 0; n <= maxCode; L++)
	{
		nl[L] = n;

		for (int32_t i = 0; L - k[i] > Lmin && i < kmax; i++)
		{
			if (i == kmax - 1 && L1_long == -1)
			{
				L1_long = n;
			}

			nlk[L][k[i]] = n - nl[L - k[i] - 1];

			for (int32_t j = nl[L - k[i] - 1]; j < nl[L - k[i]] && n <= maxCode; j++, n++)
			{
			}
		}
		if (L1_long_prev > -1)
		{
			for (int32_t j = L1_long_prev; j < nl[L] && n <= maxCode; j++, n++)
			{
			}
		}
		if (t < kmax && L - 1 != k[t])
		{
			n++;
		}
		else
		{
			t++;
		}
		L1_long_prev = L1_long;
		L1_long = -1;
	}
}

// R2,4-infinity automaton modelling
ai_state a_step24infty(ai_state prev, int32_t bit)
{
	state_max = 5;
	ai_state r = { 0, prev.L + 1, prev.p, -1 };
	if (bit == 0)
	{
		switch (prev.st)
		{
		case 0: r.p = prev.p + nlk[prev.L][0];
			break;
		case 1: r.p = prev.p + nlk[prev.L][1];
			break;
		case 2: r.L = 4;
			r.res = prev.p;
			r.p = 0;
			break;
		case 3: r.p = prev.p + nlk[prev.L][3];
			break;
		case 4: r.p = nl[r.L] - 1;
			break;
		}
	}
	else
	{
		r.st = prev.st < 4 ? prev.st + 1 : 4;

		if (prev.st == 3)
		{
			r.L = 5;
			r.res = prev.p;
			r.p = 0;
		}
	}
	return r;
}

void formTrmd_inner1_new1(int32_t L, ai_state state)
{
	tabsrmd1 v = { 0, 0, 0, 0, 0, nullptr };
	ai_state a = state;
	for (uint32_t x = 0; x < 256; x++)
	{
		state = a;
		v.p1 = v.p2 = v.p3 = v.p4 = -1;
		for (uint32_t k = 128; k > 0; k >>= 1)
		{
			state = a_step24infty(state, x & k);

			if (state.res != -1)
			{
				if (v.p1 == -1)
				{
					v.p1 = state.res;
				}
				else
				{
					if (v.p2 == -1)
					{
						v.p2 = state.res;
					}
					else
					{
						v.p3 = state.res;
					}
				}
				v.n++;
			}
		}

		if (v.p1 == -1)
		{
			v.p1 = state.p;
		}
		else
		{
			if (v.p2 == -1)
			{
				v.p2 = state.p;
			}
			else
			{
				if (v.p3 == -1)
				{
					v.p3 = state.p;
				}
				else
				{
					v.p4 = state.p;
				}
			}
		}

		pack_new((state_max * a.L + a.st) * 256 + x, v, state);
	}
}

void formTrmd1_new()
{
	state_max = 3;
	for (int32_t a = 0; a < state_max; a++)
	{
		for (int32_t L = 0; L < 45; L++)
		{
			ai_state state = { a,L,0,-1 };
			formTrmd_inner1_new1(L, state);
		}
	}
}

void precalc1(uint32_t maxCode)
{
	genRMD1_nl_nlk(maxCode);
	formTrmd1_new();
}

void decodeRMD(uint8_t* file, uint32_t fileN, uint32_t*& codes, uint32_t& codesN)
{
	uint32_t* file32 = (uint32_t*)file;
	codesN = file32[0];
	codes = new uint32_t[codesN + 8];

	uint32_t maxCode = file32[1];

	measure_start();

	uint64_t t, z;
	uint16_t v;
	int32_t temp = 0, k = 0, l = 0, tr = 0;
	uint32_t x;
	uint16_t ptr = 0;

	for (int32_t i = 2 * sizeof(uint32_t); i < fileN; i++)
	{
		v = (ptr << 8) + *(file + i);

		int32_t oldK = k;

		k += rmd_n[v];

		*(uint64_t*)(codes + oldK) = rmd_numbers[v] + tr;
		*(uint64_t*)(codes + oldK + 2) = extra_rmd_numbers[v];

		ptr = ptr_rmd[v];

		tr = *(codes + k);
	}

	measure_end();

	memmove(codes, codes + 1, sizeof(uint32_t) * codesN);
}


// main
int main(int argc, char** argv)
{
	if (argc != 5)
	{
		cout << "Error\n";
		return 0;
	}

	cout << "Start RMD_R2_4inf " << argv[2] << "\n";

	uint32_t iters = stoi(argv[1]);

	// encode
	uint32_t* codes;
	uint32_t codesN;
	readCodes(string(argv[2]), codes, codesN);

	uint8_t* file;
	uint32_t fileN;

	encodeCodes1(codes, codesN, file, fileN);

	delete[] codes;

	writeFile(string(argv[3]), file, fileN);

	cout << fileN << "\n";

	delete[] file;

	// decode
	uint8_t* d_file;
	uint32_t d_fileN;
	readFile(string(argv[3]), d_file, d_fileN);

	precalc1(10000000);

	uint32_t d_codesN;

	double mnTime = 1000.0;
	double fullTime = 0.0;

	for (int32_t iter = 0; iter < iters; iter++)
	{
		uint32_t* d_codes;

		decodeRMD(d_file, d_fileN, d_codes, d_codesN);

		delete[] d_codes;

		mnTime = m_time < mnTime ? m_time : mnTime;
		fullTime += m_time;
	}

	cout.precision(4);
	cout << fixed << mnTime * 1000.0 << " " << fixed << fullTime / iters * 1000.0 << "\n";

	uint32_t* d_codes;

	decodeRMD(d_file, d_fileN, d_codes, d_codesN);

	delete[] d_file;

	writeCodes(string(argv[4]), d_codes, d_codesN);

	delete[] d_codes;

	return 0;
}
