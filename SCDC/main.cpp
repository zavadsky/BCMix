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
void encodeCodes(uint32_t* codes, uint32_t codesN, uint8_t*& file, uint32_t& fileN, uint32_t S)
{
	uint32_t C = 256 - S;

	int32_t i, cur, x, j = 8;
	uint8_t temp[10];

	file = new uint8_t[codesN * sizeof(uint32_t) + 12];
	*(uint32_t*)file = codesN;
	*(uint32_t*)(file + sizeof(uint32_t)) = S;

	for (i = 0; i < codesN; i++)
	{
		cur = 9;
		temp[cur] = C + codes[i] % S;
		cur--;
		x = codes[i] / S;

		while (x > 0)
		{
			x--;
			temp[cur] = x % C;
			cur--;
			x /= C;
		}

		for (int32_t t = cur + 1; t < 10; t++)
		{
			file[j] = temp[t];
			j++;
		}
	}

	fileN = j;
}

// decode text
#define measure_start() LARGE_INTEGER start, finish, freq; \
	QueryPerformanceFrequency(&freq); \
	QueryPerformanceCounter(&start);

#define measure_end() QueryPerformanceCounter(&finish); \
					m_time = ((finish.QuadPart - start.QuadPart) / (double)freq.QuadPart);

void decode_scdc(uint8_t* file, uint32_t fileN, uint32_t*& codes, uint32_t& codesN)
{
	codesN = *(uint32_t*)file;
	codes = new uint32_t[codesN];

	measure_start();

	uint32_t S, C;
	S = *(uint32_t*)(file + sizeof(uint32_t));
	C = 256 - S;

	int32_t i, k = 0;
	uint8_t* fileP;

	int32_t tot, byte, cur, base;

	i = 2 * sizeof(uint32_t);

	for (; i < fileN; i += byte + 1)
	{
		fileP = file + i;

		cur = byte = base = 0;
		tot = S;

		while (fileP[byte] < C)
		{
			cur = cur * C + fileP[byte];
			byte++;
			base = base + tot;
			tot = tot * C;
		}

		cur = cur * S + fileP[byte] - C;
		codes[k] = cur + base;
		k++;
	}

	measure_end();
}

void check(uint32_t* codes,uint32_t* decoded,uint32_t codesN) {
int i;
    for(i=0;i<codesN && codes[i]==decoded[i];i++);
    cout<<"Check failed at: "<<i<<"; N="<<codesN<<endl;
}

// main
int main(int argc, char** argv)
{
	if (argc != 5)
	{
		cout << "Error\n";

		return 0;
	}

	uint32_t S;

	cout << "Start SCDC " << argv[2] << "\n";

	uint32_t iters = stoi(argv[1]);

	// encode
	uint32_t* codes;
	uint32_t codesN;
	readCodes(string(argv[2]), codes, codesN);

	if (codesN < 28000)
	{
		S = 234;
	}
	else if (codesN < 200000)
	{
		S = 205;
	}
	else if (codesN < 1000000)
	{
		S = 197;
	}
	else if(codesN < 15000000)
	{
		S = 175;
	}
	else
	{
		S = 165;
	}

	uint8_t* file;
	uint32_t fileN;

	encodeCodes(codes, codesN, file, fileN, S);

//	delete[] codes;

	writeFile(string(argv[3]), file, fileN);

	delete[] file;

	// decode
	uint8_t* d_file;
	uint32_t d_fileN;
	readFile(string(argv[3]), d_file, d_fileN);

	uint32_t d_codesN;

	double mnTime = 1000.0;
	double fullTime = 0.0;

	for (int32_t iter = 0; iter < iters; iter++)
	{
		uint32_t* d_codes;

		decode_scdc(d_file, d_fileN, d_codes, d_codesN);

		delete[] d_codes;

		mnTime = m_time < mnTime ? m_time : mnTime;
		fullTime += m_time;

		cout << ".";
	}

	cout.precision(4);
	cout << fixed << mnTime * 1000.0 << " " << fixed << fullTime / iters * 1000.0 << "\n";

	uint32_t* d_codes;

	decode_scdc(d_file, d_fileN, d_codes, d_codesN);

	check(codes,d_codes,d_codesN);

	delete[] d_file;

	writeCodes(string(argv[4]), d_codes, d_codesN);

	delete[] d_codes;

	return 0;
}
