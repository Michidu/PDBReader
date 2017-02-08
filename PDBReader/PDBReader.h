#pragma once

#include "dia2.h"

const char* const rgBaseType[] =
{
	"<NoType>", // btNoType = 0,
	"void", // btVoid = 1,
	"char", // btChar = 2,
	"wchar_t", // btWChar = 3,
	"signed char",
	"unsigned char",
	"int", // btInt = 6,
	"unsigned int", // btUInt = 7,
	"float", // btFloat = 8,
	"<BCD>", // btBCD = 9,
	"bool", // btBool = 10,
	"short",
	"unsigned short",
	"long", // btLong = 13,
	"unsigned long", // btULong = 14,
	"__int8",
	"__int16",
	"__int32",
	"__int64",
	"__int128",
	"unsigned __int8",
	"unsigned __int16",
	"unsigned __int32",
	"unsigned __int64",
	"unsigned __int128",
	"<currency>", // btCurrency = 25,
	"<date>", // btDate = 26,
	"VARIANT", // btVariant = 27,
	"<complex>", // btComplex = 28,
	"<bit>", // btBit = 29,
	"BSTR", // btBSTR = 30,
	"HRESULT" // btHresult = 31
};

bool LoadDataFromPdb(IDiaDataSource**, IDiaSession**, IDiaSymbol**);
bool ReadJson();
void DumpStructs(IDiaSymbol*);
char* ConvertBSTRToLPSTR(BSTR);
void DumpType(IDiaSymbol*, const std::string&, int);
void DumpData(IDiaSymbol*, const std::string&);
std::string GetName(IDiaSymbol*);
void DumpFunction(IDiaSymbol*, const std::string&);
void WriteJson();
void Cleanup(IDiaSymbol*, IDiaSession*);
