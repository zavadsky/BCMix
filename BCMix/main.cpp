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


uint8_t bestDigitsSz[24];
uint32_t bestBlocksNum;
uint64_t bestFullBitsSz;

uint64_t getSum(uint32_t l, uint32_t r, uint32_t* pref, uint32_t maxCode)
{
	uint64_t res = 0;

	res += r > maxCode ? pref[maxCode] : pref[r];
	res -= l == 0 ? 0 : pref[l - 1];

	return res;
}

#define shortestDigitLen 2
#define longestDigitLen 4

// Searching the optimal code - Best Code Search
void BCS(uint8_t dSz[24], uint32_t dN, uint64_t fullN,
	uint64_t incN, uint64_t cLen, uint64_t fullSz, uint32_t* PSum, uint32_t maxC)
{
	if (dN > 4) // all digits starting from 5th = 2
	{
		if (fullN > maxC)
		{
			dSz[dN] = 2;

			if (fullSz < bestFullBitsSz)
			{
				bestFullBitsSz = fullSz;
				bestBlocksNum = dN;
				for (uint32_t i = 0; i < 24; i++)
				{
					bestDigitsSz[i] = dSz[i];
				}
			}
			return;
		}

		dSz[dN] = 2;
		uint64_t newSz = fullSz + (cLen + 2) * getSum(fullN, fullN + incN - 1, PSum, maxC);
		BCS(dSz, dN + 1, fullN + incN, incN * 3, cLen + 2, newSz, PSum, maxC);
	}
	else // lines 4-7 from Alg. 3 i the paper
        for(int i=shortestDigitLen;i<=longestDigitLen;i++) {
            dSz[dN] = i;
            uint64_t newSz = fullSz + (cLen + i) * getSum(fullN, fullN + incN - 1, PSum, maxC);
            BCS(dSz, dN + 1, fullN + incN, incN * ((1 << i)-1), cLen + i, newSz, PSum, maxC);
		}
}

// Precalc for searching the optimal code
void precalcBestBlocks(uint32_t* codes, uint32_t codesN, uint32_t maxCode)
{
	uint32_t* PSum = new uint32_t[maxCode + 1];
	memset(PSum, 0, sizeof(uint32_t) * (maxCode + 1));

    // Calculate prefix sums
	for (uint32_t i = 0; i < codesN; i++)
	{
		PSum[codes[i]]++;
	}
	for (uint32_t i = 1; i <= maxCode; i++)
	{
		PSum[i] += PSum[i - 1];
	}

	bestFullBitsSz = UINT64_MAX;
	uint8_t digitLen[24]; // Bit lengths of digits of optimal code
	memset(digitLen, 2, sizeof(uint8_t) * 24);

	BCS(digitLen, 0, 0, 1, 0, 0, PSum, maxCode); // Call the optimal search function

	delete[] PSum;
}

uint32_t* myCode;
uint32_t* myCodeSz;

// Construct the code of maxCode codewords and digit bit lengths in the array bestDigitsSz
void precalcCodesAndSizes(uint32_t maxCode)
{
	myCode = new uint32_t[maxCode + 1];
	myCodeSz = new uint32_t[maxCode + 1];

	// Building arrays Pow and mask
	uint32_t Pow[24], blocksMask[24];
	Pow[0] = 1;
	for (uint32_t i = 1; i <= bestBlocksNum + 1; i++)
	{
		blocksMask[i - 1] = (1 << bestDigitsSz[i - 1]) - 1;
		Pow[i] = Pow[i - 1] * blocksMask[i - 1];
	}

	// Constructing the array of codewords myCode and the array of codeword lengths myCodeSz
    uint32_t cum = 0, pwsI = 0, len = bestDigitsSz[0];
	for (uint32_t i = 0; i <= maxCode; i++)
	{
		if (cum >= Pow[pwsI])
		{
			cum -= Pow[pwsI];
			pwsI++;
			len += bestDigitsSz[pwsI];
		}

		uint32_t code = 0, shift = 0, cumCopy = cum;
		for (uint32_t j = 0; j < pwsI; j++)
		{
			uint32_t val = cumCopy % blocksMask[j];
			cumCopy /= blocksMask[j];
			code |= val << shift;
			shift += bestDigitsSz[j];
		}
		code |= blocksMask[pwsI] << shift;

		myCode[i] = code;
		myCodeSz[i] = shift + bestDigitsSz[pwsI];

		cum++;
	}
}

void precalcClear()
{
	delete[] myCode;
	delete[] myCodeSz;
}

// encode text
// codes - array of numbers to be encoded; codesN - array length; file - encoded bitstream; fileN - byte length of the bitstream
void encodeCodes(uint32_t* codes, uint32_t codesN, uint8_t*& file, uint32_t& fileN)
{
	uint32_t maxCode = 0;
	for (int i = 0; i < codesN; i++)
	{
		maxCode = maxCode > codes[i] ? maxCode : codes[i];
	}

	precalcBestBlocks(codes, codesN, maxCode); // searching the optimal code

	precalcCodesAndSizes(maxCode); // constructing the code

	// calculating bit size of the compressed file
	uint64_t fullSizeBits = 0;
	for (int i = 0; i < codesN; i++)
	{
		fullSizeBits += myCodeSz[codes[i]];
	}

	fileN = (fullSizeBits + 7) / 8 + 16;
	fileN += sizeof(uint32_t) * 2 + sizeof(uint8_t) * 4; // size of the compressed file in bytes

	file = new uint8_t[fileN];
	memset(file, 0, sizeof(uint8_t) * fileN);

	cout << "file size = " << fileN << "\n";

	uint32_t* file32 = (uint32_t*)file;
	// write to file: 1) number of elements; 2) maximal codeword; 3)-6) first 4 digits bit lengths
	file32[0] = codesN;
	file32[1] = maxCode;
	file[sizeof(uint32_t) * 2 + 0] = bestDigitsSz[0];
	file[sizeof(uint32_t) * 2 + 1] = bestDigitsSz[1];
	file[sizeof(uint32_t) * 2 + 2] = bestDigitsSz[2];
	file[sizeof(uint32_t) * 2 + 3] = bestDigitsSz[3];

	uint32_t pos = 3;
	uint64_t curCode = 0;
	uint32_t curSz = 0;

	uint32_t cnt[3] = { 0, 0, 0 };

    // write to file sequence of codewords
	for (uint32_t i = 0; i < codesN; i++)
	{
		uint64_t code = myCode[codes[i]];
		uint32_t sz = myCodeSz[codes[i]];

		curCode |= code << curSz;
		curSz += sz;

		if (curSz >= 32)
		{
			file32[pos] = (uint32_t)curCode;
			pos++;
			curCode >>= 32;
			curSz -= 32;
		}
	}
	if (curSz != 0)
	{
		file32[pos] = (uint32_t)curCode;
	}

	precalcClear();
}

// decode text
#define measure_start() LARGE_INTEGER start, finish, freq; \
	QueryPerformanceFrequency(&freq); \
	QueryPerformanceCounter(&start);

#define measure_end() QueryPerformanceCounter(&finish); \
					m_time = ((finish.QuadPart - start.QuadPart) / (double)freq.QuadPart);

#define blockLen (10)               // bit length of a decoding block
#define microIters (55 / blockLen)  // number of iterations in decoding of 64-bit words
#define blockSz (1 << blockLen)
#define blockMask (blockSz - 1)
#define tableN 3
#define NblockSz blockSz*tableN

struct TableDecode
{
	uint32_t L[blockSz];
	uint8_t n[blockSz];
	uint8_t shift[blockSz];

	TableDecode* nxtTable[blockSz];
};

TableDecode ts[3]; // Structure of decoding tables. Each codeword consists of not more than 3 parts

// precalc 3 tables for fast decoding
void buidTableDecode(uint32_t digitLen[16])
{
	// extra constants from the paper:
	// mask (consisting of all ones);
	// number of possible values of digit (at the same time)
	// value that indicates delimiter (at the same time)
	uint32_t masks[16];
	// the number of i-digit codewords
	uint32_t pws[16];
	// the number of codewords consisting of less than i digits (pref_i in paper)
	uint32_t Pref[16];

	// precalc extra constants according to formular from paper
	for (uint32_t i = 0; i < 16; i++)
	{
		masks[i] = (1 << digitLen[i]) - 1;
	}
	pws[0] = 1;
	Pref[0] = 0;
	for (uint32_t i = 1; i < 16; i++)
	{
		pws[i] = pws[i - 1] * masks[i - 1];
		Pref[i] = Pref[i - 1] + pws[i - 1];
	}

	// meaning if next variables (1-indexing):
	// * digits [1; block1I] belong to first decoding block
	// * digits [block1I + 1; block2I] belong to second decoding block
	// * digits [block2I + 1; block3I] belong to third decoding block
	uint32_t block1I, block2I, block3I, blockI = 0;
	uint32_t blockLenSum;

	// compute digits in first decoding block (total length of digits <= blockLen)
	blockLenSum = 0;
	while (blockLenSum + digitLen[blockI] <= blockLen)
	{
		blockLenSum += digitLen[blockI];
		blockI++;
	}
	block1I = blockI;

	// compute digits in second decoding block (total length of digits <= blockLen)
	blockLenSum = 0;
	while (blockLenSum + digitLen[blockI] <= blockLen)
	{
		blockLenSum += digitLen[blockI];
		blockI++;
	}
	block2I = blockI;

	// compute digits in third decoding block (total length of digits <= blockLen)
	blockLenSum = 0;
	while (blockLenSum + digitLen[blockI] <= blockLen)
	{
		blockLenSum += digitLen[blockI];
		blockI++;
	}
	block3I = blockI;

	// precalc first table (for all possible bitmasks of length blockLen)
	// reminder: blockSz = 2^blockLen
	for (uint32_t i = 0; i < blockSz; i++)
	{
		// for bitmask i

		// variables:
		// * myL - decoded value stored in prefix (before delimiter or end of block)
		// * myN - 0 if no delimiter inside block, 1 otherwise
		// * myShift - how many bits of bitmask is processed by table
		// * tShift - auxiliary variable to iterate through digits in bitmask
		uint32_t myL = 0, myN = 0, myShift = 0, tShift = 0;

		// iterate through digits of bitmask
		for (uint32_t j = 0; j < block1I; j++)
		{
			myShift += digitLen[j];

			// val - value of current digit
			uint32_t val = (i >> tShift) & masks[j];

			// if val is delimiter
			if (val == masks[j])
			{
				// process value of delimiter according to formulas from paper
				// and stop decoding block
				myN = 1;
				myL += Pref[j];
				break;
			}
			else
			{
				// process value of digit according to formulas from paper
				myL += val * pws[j];
			}

			tShift += digitLen[j];
		}

		// write computed values to lookup tables
		ts[0].L[i] = myL;
		ts[0].n[i] = myN;
		ts[0].shift[i] = myShift;
		ts[0].nxtTable[i] = myN? &ts[0] : &ts[1];

	}

	// precalc second table (for all possible bitmasks of length blockLen)
	// works in the same way as for the first table
	for (uint32_t i = 0; i < blockSz; i++)
	{
		uint32_t myL = 0, myN = 0, myShift = 0, tShift = 0;

		// make sure to use correct digits as it is second block
		for (uint32_t j = block1I; j < block2I; j++)
		{
			myShift += digitLen[j];

			uint32_t val = (i >> tShift) & masks[j];

			if (val == masks[j])
			{
				myN = 1;
				myL += Pref[j];
				break;
			}
			else
			{
				myL += val * pws[j];
			}

			tShift += digitLen[j];
		}

		ts[1].L[i] = myL;
		ts[1].n[i] = myN;
		ts[1].shift[i] = myShift;
		ts[1].nxtTable[i] = myN? &ts[0] : &ts[2];

	}

	// precalc third table (for all possible bitmasks of length blockLen)
	// works in the same way as for the first table
	for (uint32_t i = 0; i < blockSz; i++)
	{
		uint32_t myL = 0, myN = 0, myShift = 0, tShift = 0;

		// make sure to use correct digits as it is third block
		for (uint32_t j = block2I; j < block3I; j++)
		{
			myShift += digitLen[j];

			uint32_t val = (i >> tShift) & masks[j];

			if (val == masks[j])
			{
				myN = 1;
				myL += Pref[j];
				break;
			}
			else
			{
				myL += val * pws[j];
			}

			tShift += digitLen[j];
		}

		ts[2].L[i] = myL;
		ts[2].n[i] = myN;
		ts[2].shift[i] = myShift;
		ts[2].nxtTable[i] = myN? &ts[0] : &ts[2];

	}
}

// Fast decoding method, Alg. 4 in the paper
// file - bitstream to be decoded, fileN - bitstream length in bytes
// codes - array to store decoded numbers, codesN - number of elements in codes array
void decode(uint8_t* file, uint32_t fileN, uint32_t*& codes, uint32_t& codesN)
{
	uint32_t* file32 = (uint32_t*)file;
	uint8_t* file8;
	codesN = file32[0];
	codes = new uint32_t[codesN + 4];

	uint32_t maxCode = file32[1];

	// First 4 digits of a code are stored in file, the rest = 2
	uint32_t digitLen[16];
	for (auto& it : digitLen)
	{
		it = 2;
	}
	digitLen[0] = file[sizeof(uint32_t) * 2 + 0];
	digitLen[1] = file[sizeof(uint32_t) * 2 + 1];
	digitLen[2] = file[sizeof(uint32_t) * 2 + 2];
	digitLen[3] = file[sizeof(uint32_t) * 2 + 3];

	file8 = (uint8_t*)file32;

	uint64_t bitStream = 0;
	uint32_t bitLen = 0, codesPos = 0, file8Pos = 12;
	TableDecode* table = &ts[0];

	buidTableDecode(digitLen);
	measure_start();

	memset(codes, 0, sizeof(uint32_t) * (codesN + 4));

	while (codesPos < codesN) //line 4
	{
		uint32_t freeBytes = (64 - bitLen) >> 3; //6
		bitStream |= (*(uint64_t*)(void*)(file8 + file8Pos)) << bitLen; //5
		bitLen += freeBytes << 3; //7
		file8Pos += freeBytes; //8

		for (uint32_t iter = 0; iter < microIters; iter++) //9
		{
			uint32_t block = bitStream & blockMask; //10

			uint32_t _shift = table->shift[block]; // getting tb.shift

			bitStream >>= _shift; //11
			bitLen -= _shift; //12

			codes[codesPos] += table->L[block]; //13

			codesPos += table->n[block]; //14

			table = table->nxtTable[block]; //15
		}
	}

	measure_end();
}

// Check the correctness of the decoding
void check(uint32_t* codes, uint32_t* decoded, uint32_t codesN) {
	int i;
	for (i = 0; i < codesN && codes[i] == decoded[i]; i++);
	if (i < codesN-1)
        cout << "Check failed at: " << i << "; N=" << codesN << endl;
	else
        cout << "Decoded successfully" << endl;
}


int main(int argc, char** argv)
{
	if (argc != 5)
	{
		cout << "Error\n";
		return 0;
	}

	cout << "Start BCMix_singleCodeword decoding, 10 bit block. File: " << argv[2] << "\n";

	uint32_t iters = stoi(argv[1]);

	// encode
	uint32_t* codes;
	uint32_t codesN;
	readCodes(string(argv[2]), codes, codesN); //reading numbers from file

	uint8_t* file;
	uint32_t fileN;

	encodeCodes(codes, codesN, file, fileN); //encoding

	writeFile(string(argv[3]), file, fileN); //writing code parameters and encoded bitstream to file

	delete[] file;

	cout << "BCMix code structure = " << int(bestDigitsSz[0]) << int(bestDigitsSz[1])
		<< int(bestDigitsSz[2]) << int(bestDigitsSz[3]) << int(bestDigitsSz[4]) << "\n";

	// decode
	uint8_t* d_file;
	uint32_t d_fileN;

	readFile(string(argv[3]), d_file, d_fileN); //reading code parameters and encoded bitstream from file

	uint32_t d_codesN;

	double mnTime = 10000.0;
	double fullTime = 0.0;

	for (int32_t iter = 0; iter < iters; iter++) //experiment on decoding speed
	{
		uint32_t* d_codes;

		decode(d_file, d_fileN, d_codes, d_codesN);

		delete[] d_codes;

		mnTime = m_time < mnTime ? m_time : mnTime;
		fullTime += m_time;
		cout << ".";
	}

	cout.precision(4);
	cout << endl << "min time=" << fixed << mnTime * 1000.0 << "; avg time=" << fixed << fullTime / iters * 1000.0 << "\n";

	uint32_t* d_codes;

    //check the correctness of decoding
	decode(d_file, d_fileN, d_codes, d_codesN);
	check(codes, d_codes, d_codesN);

	delete[] d_file;

	writeCodes(string(argv[4]), d_codes, d_codesN); //write decoded numbers to file

	delete[] d_codes;

	return 0;
}
