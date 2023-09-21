#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <iostream>
#include <windows.h>
#include <stdint.h>
#include <set>
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

/*

Huffman tree is stored in the following way:
tree[0][i] - left child of node i (0-bit transition)
tree[1][i] - right child of node i (1-bit transition)
Root of huffman tree is tree[*][0]
All non-negative indexes of array 'tree' link to non-leaf nodes of huffman tree
If tree[*][index] < 0, then it is link to a leaf node, and -(tree[*][index] + 1) is the output code corresponding to this leaf node

2 additional array are used for tree construction and encoding:
leafPar[code] - parent of leaf node that corresponds to 'code'. Such indirect way of storing parent is used due to negative indexes of non-lef nodes
nodePar[i] - parent of non-leaf node i

*/

// encode text
int32_t* tree[2];
int32_t* leafPar, *nodePar;

void createTree(uint32_t* codes, uint32_t codesN)
{
	// find max code
	uint32_t maxCode = 0;
	for (uint32_t i = 0; i < codesN; i++)
	{
		maxCode = maxCode > codes[i] ? maxCode : codes[i];
	}

	// calculate frequency of each code
	uint32_t* cnt = new uint32_t[maxCode + 1];
	memset(cnt, 0, sizeof(uint32_t) * (maxCode + 1));

	for (int i = 0; i < codesN; i++)
	{
		cnt[codes[i]]++;
	}

	tree[0] = new int32_t[maxCode];
	tree[1] = new int32_t[maxCode];
	leafPar = new int32_t[maxCode + 1];
	nodePar = new int32_t[maxCode];
	nodePar[0] = -1;

	int32_t treePos = maxCode - 1;

	// create auxiliary set for nulding huffman tree and fill it with initial frequncies of codes
	set<pair<int32_t, int32_t> > s; // (cnt, i)
	for (int i = 0; i < maxCode + 1; i++)
	{
		s.insert(make_pair(cnt[i], -i - 1));
	}

	// build huffman tree
	while (s.size() > 1)
	{
		// take 2 nodes with minimal frequencies
		pair<int32_t, int32_t> a = *s.begin();
		s.erase(s.begin());
		pair<int32_t, int32_t> b = *s.begin();
		s.erase(s.begin());

		tree[0][treePos] = a.second;
		tree[1][treePos] = b.second;

		if (a.second < 0)
		{
			// is a.second links to leaf node
			leafPar[-(a.second + 1)] = treePos;
		}
		else
		{
			// is a.second links to non-leaf node
			nodePar[a.second] = treePos;
		}
		if (b.second < 0)
		{
			// is a.second links to leaf node
			leafPar[-(b.second + 1)] = treePos;
		}
		else
		{
			// is a.second links to non-leaf node
			nodePar[b.second] = treePos;
		}

		// create new node with frequency equal to sum of frequencies of a and b
		s.insert(make_pair(a.first + b.first, treePos));

		treePos--;
	}

	delete[] cnt;
}

void encodeCodes(uint32_t* codes, uint32_t codesN, uint8_t*& file, uint32_t& fileN)
{
	uint32_t maxCode = 0;
	for (int i = 0; i < codesN; i++)
	{
		maxCode = maxCode > codes[i] ? maxCode : codes[i];
	}

	cout << "maxCode = " << maxCode << "\n";

	// calculate total size of output file
	uint64_t bitsCnt = 0;

	for (int32_t i = 0; i < codesN; i++)
	{
		int32_t code = codes[i];

		int32_t par = leafPar[code];
		while (par != -1)
		{
			par = nodePar[par];
			bitsCnt++;
		}
	}

	fileN = (bitsCnt + 7) / 8;
	fileN += sizeof(uint32_t);
	file = new uint8_t[fileN];
	memset(file, 0, sizeof(uint8_t) * fileN);

	uint32_t* file32 = (uint32_t*)file;
	file32[0] = codesN;

	uint32_t bitPos = 32;

	// encode each code separately
	for (int32_t i = 0; i < codesN; i++)
	{
		int32_t code = codes[i];
		uint32_t bits = 2;

		int32_t par = leafPar[code];
		// process leaf node separately
		if (tree[1][par] == -(code + 1))
		{
			bits |= 1;
		}
		// collect all other bits of huffman code
		while (true)
		{
			int32_t newPar = nodePar[par];
			if (newPar == -1) { break; }

			if (tree[0][newPar] == par)
			{
				bits <<= 1;
			}
			else
			{
				bits = (bits << 1) | 1;
			}
			par = newPar;
		}

		// write huffman code to output
		while (bits > 1)
		{
			uint32_t bit = bits & 1;
			bits >>= 1;

			file[bitPos >> 3] |= bit << (bitPos & 7);
			bitPos++;
		}
	}
}

// decode text
#define measure_start() LARGE_INTEGER start, finish, freq; \
	QueryPerformanceFrequency(&freq); \
	QueryPerformanceCounter(&start);

#define measure_end() QueryPerformanceCounter(&finish); \
					m_time = ((finish.QuadPart - start.QuadPart) / (double)freq.QuadPart);

void decode(uint8_t* file, uint32_t fileN, uint32_t*& codes, uint32_t& codesN)
{
	uint32_t* file32 = (uint32_t*)file;

	codesN = file32[0];
	codes = new uint32_t[codesN];

	measure_start();

	int32_t bitPos = 32, treePos;

	// decode each code separately
	for (int32_t i = 0; i < codesN; i++)
	{
		// start from root of huffman tree
		treePos = 0;

		// while we are not in leaf node just traverse tree
		while (treePos >= 0)
		{
			uint32_t bit = (file[bitPos >> 3] >> (bitPos & 7)) & 1;
			bitPos++;

			treePos = tree[bit][treePos];
		}

		// write decoded code to output
		codes[i] = -treePos - 1;
	}

	measure_end();
}


int main(int argc, char** argv)
{
	//*
	if (true)
	{
		argc = 5;

		//argv[1] = (char*)"500";
		//argv[1] = (char*)"50";
		argv[1] = (char*)"10";

		//argv[2] = (char*)"../data/bible.txt_codes";
		//argv[2] = (char*)"../data/ternary.200M_codes";
		argv[2] = (char*)"../data/enwik9_codes";

		argv[3] = (char*)"../data/file.encoded";
		argv[4] = (char*)"../data/file.decoded";
	}
	//*/

	if (argc != 5)
	{
		cout << "Error\n";

		return 0;
	}

	cout << "Start Huff " << argv[2] << "\n";

	uint32_t iters = stoi(argv[1]);

	// encode
	uint32_t* codes;
	uint32_t codesN;
	readCodes(string(argv[2]), codes, codesN);

	cout << codesN << " codeword read." << endl;

	createTree(codes, codesN);

	uint8_t* file;
	uint32_t fileN;

	encodeCodes(codes, codesN, file, fileN);

	delete[] codes;

	writeFile(string(argv[3]), file, fileN);

	cout << fileN << "\n";

	delete[] file;

	// decode
	uint8_t* d_file;
	uint32_t d_fileN;
	readFile(string(argv[3]), d_file, d_fileN);

	uint32_t d_codesN;

	double mnTime = 1000000.0;
	double fullTime = 0.0;

	for (int32_t iter = 0; iter < iters; iter++)
	{
		uint32_t* d_codes;

		decode(d_file, d_fileN, d_codes, d_codesN);

		delete[] d_codes;

		mnTime = m_time < mnTime ? m_time : mnTime;
		fullTime += m_time;
	}

	cout.precision(4);
	cout << fixed << mnTime * 1000.0 << " " << fixed << fullTime / iters * 1000.0 << "\n";

	uint32_t* d_codes;

	decode(d_file, d_fileN, d_codes, d_codesN);

	delete[] d_file;

	writeCodes(string(argv[4]), d_codes, d_codesN);

	delete[] d_codes;

	return 0;
}
