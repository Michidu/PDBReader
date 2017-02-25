#include <iostream>
#include <fstream>
#include <atlcomcli.h>
#include <Pathcch.h>
#include "PDBReader.h"
#include "json.hpp"

#pragma comment(lib, "Pathcch.lib")

const char* fileName = "ShooterGameServer.exe";

//TODO: Get rid of json
nlohmann::json jsonConfig, jsonDump;

int main(int argc, char* argv[])
{
	std::wstring fullPath = GetCurrentDir() + L"/ShooterGameServer.pdb";

	std::ifstream f(fullPath);
	if (!f.good())
	{
		std::cerr << "Failed to open pdb file";
		return -1;
	}

	IDiaDataSource* pDiaDataSource;
	IDiaSession* pDiaSession;
	IDiaSymbol* pSymbol;

	if (!LoadDataFromPdb(&pDiaDataSource, &pDiaSession, &pSymbol))
	{
		std::cerr << "Failed to load data from pdb file";
		return -1;
	}

	if (!ReadJson())
	{
		return -1;
	}

	DumpStructs(pSymbol);
	WriteJson();
	Cleanup(pSymbol, pDiaSession);

	return 0;
}

std::wstring GetCurrentDir()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(nullptr, buffer, sizeof(buffer));

	PathCchRemoveFileSpec(buffer, sizeof(buffer));

	std::wstring dirPath(buffer);

	return dirPath;
}

bool LoadDataFromPdb(IDiaDataSource** ppSource, IDiaSession** ppSession, IDiaSymbol** ppSymbol)
{
	std::wstring dirPath = GetCurrentDir();

	std::wstring fullLibPath = dirPath + L"/msdia140.dll";
	HMODULE hModule = LoadLibraryW(fullLibPath.c_str());
	if (!hModule)
	{
		std::cerr << "Failed to load msdia140.dll - " << GetLastError() << std::endl;
		return false;
	}

	HRESULT (WINAPI* DllGetClassObject)(REFCLSID, REFIID, LPVOID) = reinterpret_cast<HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID)>(GetProcAddress(hModule, "DllGetClassObject"));
	if (!DllGetClassObject)
	{
		std::cerr << "Can't find DllGetClassObject - " << GetLastError() << std::endl;
		return false;
	}

	CComPtr<IClassFactory> pClassFactory;

	HRESULT hr = DllGetClassObject(__uuidof(DiaSource), IID_IClassFactory, &pClassFactory);
	if (FAILED(hr))
	{
		std::cerr << "DllGetClassObject failed - " << GetLastError() << std::endl;
		return false;
	}

	hr = pClassFactory->CreateInstance(nullptr, __uuidof(IDiaDataSource), reinterpret_cast<void **>(ppSource));
	if (FAILED(hr))
	{
		std::cerr << "CreateInstance failed - " << GetLastError() << std::endl;
		return false;
	}

	std::wstring fullPdbPath = dirPath + L"/ShooterGameServer.pdb";
	hr = (*ppSource)->loadDataFromPdb(fullPdbPath.c_str());
	if (FAILED(hr))
	{
		printf("loadDataFromPdb failed - HRESULT = %08X\n", hr);
		return false;
	}

	// Open a session for querying symbols

	hr = (*ppSource)->openSession(ppSession);
	if (FAILED(hr))
	{
		printf("openSession failed - HRESULT = %08X\n", hr);
		return false;
	}

	// Retrieve a reference to the global scope

	hr = (*ppSession)->get_globalScope(ppSymbol);
	if (hr != S_OK)
	{
		printf("get_globalScope failed\n");
		return false;
	}

	return true;
}

bool ReadJson()
{
	std::ifstream file;

	std::wstring fullPath = GetCurrentDir() + L"/config.json";
	file.open(fullPath);
	if (!file.is_open())
	{
		std::cerr << "Failed to open config.json";
		return false;
	}

	file >> jsonConfig;
	file.close();

	return true;
}

void DumpStructs(IDiaSymbol* pgSymbol)
{
	IDiaSymbol *pSymbol;

	std::cout << "Dumping structures..\n\n";

	auto structsArray = jsonConfig["structures"].get<std::vector<std::string>>();

	IDiaEnumSymbols* pEnumSymbols;

	if (FAILED(pgSymbol->findChildren(SymTagUDT, nullptr, nsNone, &pEnumSymbols)))
	{
		return;
	}

	ULONG celt = 0;

	while (SUCCEEDED(pEnumSymbols->Next(1, &pSymbol, &celt)) && celt == 1)
	{
		BSTR bstrName;
		if (pSymbol->get_name(&bstrName) != S_OK)
			continue;

		char* pszConvertedCharStr = ConvertBSTRToLPSTR(bstrName);
		std::string strName(pszConvertedCharStr);
		delete[] pszConvertedCharStr;

		// Check if structure name in config
		auto findRes = std::find(structsArray.begin(), structsArray.end(), strName);
		if (findRes != structsArray.end())
		{
			DumpType(pSymbol, strName, 0);
		}

		SysFreeString(bstrName);

		pSymbol->Release();
	}

	pEnumSymbols->Release();
}

char* ConvertBSTRToLPSTR(BSTR bstrIn)
{
	LPSTR pszOut = nullptr;

	if (bstrIn != nullptr)
	{
		int nInputStrLen = SysStringLen(bstrIn);

		int nOutputStrLen = WideCharToMultiByte(CP_ACP, 0, bstrIn, nInputStrLen, nullptr, 0, nullptr, nullptr) + 2;

		pszOut = new char[nOutputStrLen];

		memset(pszOut, 0x00, sizeof(char) * nOutputStrLen);

		WideCharToMultiByte(CP_ACP, 0, bstrIn, nInputStrLen, pszOut, nOutputStrLen, nullptr, nullptr);
	}

	return pszOut;
}

void DumpType(IDiaSymbol* pSymbol, const std::string& structure, int indent)
{
	IDiaEnumSymbols* pEnumChildren;
	IDiaSymbol* pType;
	IDiaSymbol* pChild;
	DWORD dwSymTag;
	DWORD dwSymTagType;
	ULONG celt = 0;

	if (indent > 5)
	{
		return;
	}

	if (pSymbol->get_symTag(&dwSymTag) != S_OK)
	{
		std::cerr << "ERROR - DumpType() get_symTag\n";
		return;
	}

	switch (dwSymTag)
	{
	case SymTagData:
		DumpData(pSymbol, structure);

		if (pSymbol->get_type(&pType) == S_OK)
		{
			if (pType->get_symTag(&dwSymTagType) == S_OK)
			{
				if (dwSymTagType == SymTagUDT)
				{
					DumpType(pType, structure, indent + 2);
				}
			}

			pType->Release();
		}

		break;
	case SymTagEnum:
	case SymTagUDT:
		if (SUCCEEDED(pSymbol->findChildren(SymTagNull, nullptr, nsNone, &pEnumChildren)))
		{
			while (SUCCEEDED(pEnumChildren->Next(1, &pChild, &celt)) && (celt == 1))
			{
				DumpType(pChild, structure, indent + 2);

				pChild->Release();
			}

			pEnumChildren->Release();
		}

		break;
	case SymTagFunction:
		DumpFunction(pSymbol, structure);
		break;
	default: break;
	}
}

void DumpData(IDiaSymbol* pSymbol, const std::string& structure)
{
	DWORD dwLocType;
	if (pSymbol->get_locationType(&dwLocType) != S_OK)
		return;

	if (dwLocType != LocIsThisRel)
		return;

	LONG offset;
	if (pSymbol->get_offset(&offset) != S_OK)
		return;

	BSTR bstrName;
	if (pSymbol->get_name(&bstrName) != S_OK)
		return;

	IDiaSymbol* pType;
	if (pSymbol->get_type(&pType) != S_OK)
		return;

	if (pType)
	{
		char* pszConvertedCharStr = ConvertBSTRToLPSTR(bstrName);
		std::string strName(pszConvertedCharStr);
		delete[] pszConvertedCharStr;

		jsonDump["structures"][structure][strName] = offset;
	}

	pType->Release();

	SysFreeString(bstrName);
}

std::string GetName(IDiaSymbol* pSymbol)
{
	BSTR bstrName;
	BSTR bstrUndName;
	BSTR bstrFullName;
	std::string strName;

	if (pSymbol->get_name(&bstrName) != S_OK)
		return "";

	if (pSymbol->get_undecoratedName(&bstrUndName) == S_OK)
	{
		bstrFullName = (wcscmp(bstrName, bstrUndName) == 0) ? bstrName : bstrUndName;

		SysFreeString(bstrUndName);
	}
	else
	{
		bstrFullName = bstrName;
	}

	char* pszConvertedCharStr = ConvertBSTRToLPSTR(bstrFullName);
	strName = pszConvertedCharStr;
	delete[] pszConvertedCharStr;

	SysFreeString(bstrName);
	SysFreeString(bstrFullName);

	return strName;
}

void DumpFunction(IDiaSymbol* pSymbol, const std::string& structure)
{
	DWORD dwOffset;
	if (pSymbol->get_addressOffset(&dwOffset) != S_OK)
		return;

	BSTR bstrName;
	if (pSymbol->get_name(&bstrName) != S_OK)
		return;

	char* pszConvertedCharStr = ConvertBSTRToLPSTR(bstrName);
	std::string strName(pszConvertedCharStr);
	delete[] pszConvertedCharStr;

	if (strName.find("exec") != std::string::npos) // Filter out functions with "exec" prefix
		return;

	jsonDump["structures"][structure][strName] = dwOffset;

	SysFreeString(bstrName);
}

void WriteJson()
{
	std::ofstream file;

	std::wstring fullPath = GetCurrentDir() + L"/dump.json";

	file.open(fullPath);
	file << jsonDump.dump(1);
	file.close();
}

void Cleanup(IDiaSymbol* g_pGlobalSymbol, IDiaSession* g_pDiaSession)
{
	if (g_pGlobalSymbol)
	{
		g_pGlobalSymbol->Release();
	}

	if (g_pDiaSession)
	{
		g_pDiaSession->Release();
	}

	CoUninitialize();
}
