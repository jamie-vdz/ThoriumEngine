
#define UTIL_STD_STRING
#include <iostream>
#include <string>
#include <Util/KeyValue.h>
#include "CppParser.h"
#include "Token.h"
#include "TokenParser.h"

#if _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <fstream>
#include <filesystem>
#include <sys/types.h>
#include <sys/stat.h>

#define ENGINE_VERSION "1.0"

#if _WIN32
void ParseCmdLine(LPSTR cmd, TArray<FString>& out)
{
	bool bInQoute = false;

	LPSTR ptr = cmd;
	FString curArg;

	while (true)
	{
		if (*ptr == '\0')
		{
			if (!curArg.IsEmpty())
				out.Add(curArg);
			break;
		}

		if (*ptr == '"')
		{
			bInQoute ^= 1;
		}
		else if (!bInQoute && (*ptr == ' ' || *ptr == '\t'))
		{
			out.Add(curArg);
			curArg.Clear();
		}
		else
			curArg += *ptr;

		ptr++;
	}
}

void AttachToInheritedConsole() {
	// 1. Attach to the parent process's console session
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		// 2. Re-open standard streams to point to the newly attached console
		freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
		freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
		freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

		// 3. Sync standard C++ streams (cin, cout, cerr) if using them
		std::ios::sync_with_stdio();
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd)
#else
int main(int argc, char** argv)
#endif
{
#if _WIN32
	TArray<FString> _args;
	_args.Add("Hello!!"); // this is required since 'cmdLine' doesn't include the exe path
	ParseCmdLine(cmdLine, _args);

	char* argv[128];
	int argc = (int)_args.Size();
	for (int i = 0; i < argc; i++)
		argv[i] = (char*)_args[i].c_str();

	AttachToInheritedConsole();
#endif

	// Parse arguments
	// FString cmd = GetCommandLine();
	// TArray<FString> Args;
	// Args.Resize(1);
	
	// {
	// 	bool bInQuotes = false;
	// 	bool bPrevWasSpace = false;
	// 	for (SizeType i = 0; i < cmd.Size(); i++)
	// 	{
	// 		if (cmd[i] == '"')
	// 		{
	// 			bInQuotes ^= 1;
	// 			continue;
	// 		}

	// 		if (cmd[i] == ' ' && !bInQuotes && !bPrevWasSpace)
	// 		{
	// 			Args.Add(FString());
	// 			bPrevWasSpace = true;
	// 			continue;
	// 		}

	// 		bPrevWasSpace = false;
	// 		(*Args.last()) += cmd[i];
	// 	}
	// }

	// Args.Erase(Args.begin());

	int test = 0;

	if (argc > 1)
	{
		targetPath = argv[1];
		if (targetPath[0] == ' ')
			targetPath.Erase(targetPath.begin());
		if (targetPath[targetPath.Size() - 1] == '\\' || targetPath[targetPath.Size() - 1] == '/')
			targetPath.Erase(targetPath.last());
	}
	else
	{
		std::cout << "Thorium Engine - Header Tool 1.0\n";
		return 0;
	}

	//std::cout << "Path: " << targetPath.c_str() << std::endl;

	bool bIgnoreTime = false;
	for (SizeType i = 2; i < argc; i++)
	{
		FString arg = argv[i];
		/*if (arg == "-config" && i + 1 < argc)
		{
			config = argv[i + 1];
			continue;
		}
		if (arg == "-platform" && i + 1 < argc)
		{
			platform = argv[i + 1];
			continue;
		}*/
		if (arg == "-pt" && i + 1 < argc)
		{
			ProjectType = (EProjectType)std::stoi(argv[i + 1]);
			continue;
		}
		if (arg == "-NoTimestamp")
		{
			bIgnoreTime = true;
			continue;
		}
		if (arg == "-target" && i + 1 < argc)
		{
			projectName = argv[i + 1];
			continue;
		}
		if (arg == "-test1")
		{
			test = 1;
			continue;
		}
		if (arg == "-test2")
		{
			test = 2;
			continue;
		}
	}

	FString enginePath;

	if (ProjectType == GAME_PROJECT)
	{
		FKeyValue projCfg(targetPath + "/../../config/project.cfg");
		if (!projCfg.IsOpen())
		{
			std::cerr << "error: failed to open project file!";
			return 1;
		}

#if _WIN32
		FString keyPath = "SOFTWARE\\ThoriumEngine\\" + *projCfg.GetValue("engine_version");

		HKEY hKey;
		LONG lRes = RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey);
		if (lRes == ERROR_FILE_NOT_FOUND)
			return 1;

		CHAR strBuff[MAX_PATH];
		DWORD buffSize = sizeof(strBuff);
		lRes = RegQueryValueEx(hKey, "path", 0, NULL, (LPBYTE)strBuff, &buffSize);
		if (lRes != ERROR_SUCCESS)
			return 1;

		enginePath = strBuff;
#else
		{
			std::ifstream stream(std::string(getenv("HOME")) + "/.thoriumengine/" + projCfg.GetValue("engine_version")->Value.c_str() + "/path.txt", std::ios_base::in);
			if (stream.is_open())
			{
				std::string str;
				std::getline(stream, str);

				enginePath = str;
			}
		}
#endif

		CParser::LoadModuleData(enginePath + "/build/include/engine");

		projectName = *projCfg.GetValue("game");

		/*auto* addons = projCfg.GetArray("addons");
		if (addons)
		{
			for (auto a : *addons)
			{
				bool bCore;


			}
		}*/

		//KVCategory* projects = nullptr;
		//if (ProjectType == GAME_PROJECT)
		//	projects = kv.GetCategory("games");
		//else if (ProjectType == DLC_PROJECT)
		//	projects = kv.GetCategory("dlc");

		//if (!projects)
		//{
		//	std::cerr << "error: thproj file invalid";
		//	return 1;
		//}

		//KVCategory* target = projects->GetCategory(projectName);
		//if (!target)
		//{
		//	std::cerr << "error: failed to find target in project file!";
		//	return 1;
		//}

		//KVCategory* dependencies = target->GetCategory("dependencies");
		//for (auto dep : dependencies->GetCategories())
		//{
		//	KVValue* srcPath = dep->GetValue("src");
		//	CParser::LoadModuleData(srcPath->Value);
		//}

		//targetPath.Erase(targetPath.begin() + targetPath.FindLastOf("/\\"), targetPath.end());
		//targetPath += "\\";
		//targetPath += projectName + "\\";
	}
	else if (ProjectType == LIBRARY_PROJECT)
	{
#if _WIN32
		FString keyPath = FString("SOFTWARE\\ThoriumEngine\\") + ENGINE_VERSION;

		HKEY hKey;
		LONG lRes = RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey);
		if (lRes == ERROR_FILE_NOT_FOUND)
			return 1;

		CHAR strBuff[MAX_PATH];
		DWORD buffSize = sizeof(strBuff);
		lRes = RegQueryValueEx(hKey, "path", 0, NULL, (LPBYTE)strBuff, &buffSize);
		if (lRes != ERROR_SUCCESS)
			return 1;

		enginePath = strBuff;
#else
		{
			std::ifstream stream(std::string(getenv("HOME")) + "/.thoriumengine/" + ENGINE_VERSION + "/path.txt", std::ios_base::in);
			if (stream.is_open())
			{
				std::string str;
				std::getline(stream, str);

				enginePath = str;
			}
		}
#endif

		FKeyValue kv(targetPath + "/addon.cfg");
		if (kv.IsOpen())
		{
			projectName = *kv.GetValue("identity");
		}
		else
			std::cout << "warning: failed to open addon config!\n";

		CParser::LoadModuleData(enginePath + "/build/include/engine");
	}
	else
		projectName = "Engine";
	
	GeneratedOutput = targetPath + "/Intermediate/generated";

	std::cout << "Running HeaderTool for project: " << projectName.c_str() << std::endl;

	//CreateDirectory(GeneratedOutput.c_str(), NULL);
	std::filesystem::create_directories(GeneratedOutput.c_str());

	try
	{
		FString path = targetPath + "/src/";
		for (auto entry : std::filesystem::recursive_directory_iterator(path.c_str()))
		{
			if (!entry.is_regular_file())
				continue;

			if (entry.path().extension() != ".h")
				continue;

			FHeaderData header;
			header.FileName = entry.path().stem().generic_string();
			header.FilePath = entry.path().generic_string();

			try {
				if (CTokenParser::ParseHeader(header) > 0)
				{
					std::cout << "Failed to parse header: " << header.FilePath.c_str() << std::endl;
					continue;
				}
				if (!header.bEmpty)
					Headers.Add(header);
			}
			catch (std::exception& e) { std::cerr << "error when parsing file " << header.FilePath.c_str() << ": " << e.what() << std::endl; }
		}
	}
	catch (std::exception& e) { std::cerr << "error: " << e.what() << "\n"; }

	if (test == 1)
	{
		CTokenizer tokenizer;
		std::vector<FToken> tokens;
		if (!tokenizer.ParseFile(Headers[0].FilePath.c_str(), tokens))
		{
			std::cout << "Failed to parse file: " << Headers[0].FilePath.c_str() << std::endl;
			return 0;
		}

		std::cout << "Recreated source code from tokens:\n";
		int curLine = 0;
		for (auto& t : tokens)
		{
			//std::cout << CTokenizer::TokenTypeToString(t.type) << ": '" << t.text.c_str() << "'\n";
			if (curLine != t.line)
			{
				curLine = t.line;
				std::cout << std::endl;
			}

			std::cout << t.text.c_str() << " ";
		}

		std::cout << "Tokens (" << tokens.size() << "):\n";
		for (auto& t : tokens)
			std::cout << "\t" << CTokenizer::TokenTypeToString(t.type) << ": '" << t.text.c_str() << "'\n";

		return 0;
	}

	if (test == 2)
	{
		Headers[0].classes.Clear();
		Headers[0].enums.Clear();
		CTokenParser::ParseHeader(Headers[0]);

		std::cout << Headers[0].classes.Size() << " class(es) found\n";
	}

	for (auto& h : Headers)
	{
		bool bExist = std::filesystem::exists((GeneratedOutput + "/" + h.FileName + ".generated.h").c_str()) && std::filesystem::exists((GeneratedOutput + "/" + h.FileName + ".generated.cpp").c_str());

		if (!bIgnoreTime && bExist && CParser::HeaderUpToDate(h))
			continue;

		CParser::WriteGeneratedHeader(h);
		CParser::WriteGeneratedCpp(h);
	}

	CParser::WriteModuleCpp();
	CParser::WriteModuleData();

	CParser::WriteTimestamp();

#if _WIN32
	if (IsDebuggerPresent())
	{
		std::cout << "Press enter to continue...";
		std::cin.get();
	}
#endif

	return 0;
}
