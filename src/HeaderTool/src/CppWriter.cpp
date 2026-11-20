
#include "CppParser.h"
#include <Util/FStream.h>
#include <Util/Map.h>

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

EProjectType ProjectType = ENGINE_DLL;

FString GeneratedOutput;
//FString config;
//FString platform;
FString targetPath;
FString projectName;

TArray<FTypeDefinition> PreRegisteredClasses;
TArray<FHeaderData> Headers;

struct FFlagVariable
{
	FString flag;
	bool bInverse; // if true, then the flag is added if the property doesn't have the flag.
};

static TUnorderedMap<FString, FFlagVariable> sVariableFlags = {
	{ "Editable", { "VTAG_EDITOR_EDITABLE", false }},
	{ "EditorVisible", { "VTAG_EDITOR_VISIBLE", false } },
	{ "Transient", { "VTAG_SERIALIZABLE", true } },
	{ "DontSerialize", { "VTAG_SERIALIZABLE", true } },
	{ "SaveGame", { "VTAG_SAVEGAME", false } },
	{ "IgnoreDefault", { "VTAG_IGNORE_DEFAULT", false } },

	{ "Replicated", { "VTAG_REPLICATED", false } },
	{ "NetValidate", { "VTAG_VALIDATE", false } },
	{ "ServerAuthority", { "VTAG_AUTH_SERVER", false } },
	{ "OwnerAuthority", { "VTAG_AUTH_OWNER", false } },
	{ "NoAuthority", { "VTAG_AUTH_ANY", false } },
	{ "OnlyRelevantToOwner", { "VTAG_RELEVENT_TO_OWNER", false } },
};

const char* funcCommmandTypes[] = {
	"GENERAL",
	"OUTPUT",
	"COMMAND",
	"SERVER_RPC",
	"CLIENT_RPC",
	"MULTICAST_RPC"
};

bool ClassExists(const FString& name)
{
	if (name.IsEmpty())
		return false;

	for (auto& header : Headers)
	{
		for (auto Class : header.classes)
		{
			if (Class.name == name)
				return true;
		}
	}
	for (auto& t : PreRegisteredClasses)
		if (t.name == name && t.type == 1)
			return true;

	return false;
}

FString GetVariableType(const FString& typeName)
{
	if (typeName == "TObjectPtr")
		return "EVT_OBJECT_PTR";
	if (typeName == "TArray")
		return "EVT_ARRAY";
	if (typeName == "TClassPtr")
		return "EVT_CLASS_PTR";
	if (typeName == "TEnumInt")
		return "EVT_ENUM";
	if (typeName == "TMap" || typeName == "TUnorderedMap")
		return "EVT_MAP";
	if (typeName == "void")
		return "EVT_VOID";
	if (typeName == "FString" || typeName == "WString")
		return "EVT_STRING";
	if (typeName == "SizeType" || typeName == "size_t")
		return "EVT_UINT";
	if (typeName == "float")
		return "EVT_FLOAT";
	if (typeName == "double")
		return "EVT_DOUBLE";
	if (typeName.Find("int") == 0)
		return "EVT_INT";
	if (typeName.Find("uint") == 0)
		return "EVT_UINT";
	if (typeName == "bool")
		return "EVT_BOOL";

	//if (typeName == "FOutput")
	//	return "EVT_OUTPUT";

	for (auto header : Headers)
	{
		for (auto Class : header.classes)
		{
			if (Class.name == typeName)
				return Class.classMacro.type == FMacro::STRUCT ? "EVT_STRUCT" : "EVT_CLASS";
		}
		for (auto Enum : header.enums)
		{
			if (Enum.name == typeName)
				return "EVT_ENUM";
		}
	}

	for (auto t : PreRegisteredClasses)
	{
		if (t.name == typeName)
			return t.type == 0 ? "EVT_STRUCT" : t.type == 1 ? "EVT_CLASS" : "EVT_ENUM";
	}

	return "EVT_END";
}

const FString& GetFullTypename(CppProperty& p)
{
	if (!p.fullTypename.IsEmpty())
		return p.fullTypename;

	p.fullTypename = p.typeName;
	if (p.templateArgs.Size() > 0)
	{
		p.fullTypename += "<";
		for (auto& arg : p.templateArgs)
			p.fullTypename += GetFullTypename(arg) + ",";
		p.fullTypename.Erase(p.fullTypename.end() - 1); // remove last comma
		p.fullTypename += ">";
	}

	if (p.bConst)
		p.fullTypename = "const " + p.fullTypename;
	if (p.bPointer)
		p.fullTypename += '*';
	if (p.bRef)
		p.fullTypename += '&';

	return p.fullTypename;
}

void CParser::WriteModuleCpp()
{
	std::ofstream stream((GeneratedOutput + "/" + projectName + ".init.cpp").c_str(), std::ios::trunc | std::ios::out);
	if (!stream.is_open())
		return;

	//stream << "\n#include \"Module.h\"\n\nCModule " << projectName.c_str() << "_module(\"" << projectName.c_str() << "\");\n";

	stream << "\n#include \"Module.h\"\n\n"
			<< "extern CModule& GetModule_" << projectName.c_str() << "();\n";

	//	<< "CModule& GetModule_" << projectName.c_str() << "()\n{\n"
	//	<< "\tstatic CModule _module(\"" << projectName.c_str() << "\");\n"
	//	<< "\treturn _module;\n"
	//	<< "}\n";

	//if (ProjectType != ENGINE_DLL)
		stream << std::endl << "extern \"C\" __declspec(dllexport) CModule* __GetModuleInstance() { return &GetModule_" << projectName.c_str() << "(); }\n";

	stream.close();
}

void CParser::WriteGeneratedHeader(const FHeaderData& data)
{
	FString FilePath = (GeneratedOutput + "/" + data.FileName + ".generated.h");
	std::ofstream stream(FilePath.c_str(), std::ios::trunc | std::ios::out);
	if (!stream.is_open())
		return;

	std::cout << "Writing Generated Header '" << FilePath.c_str() << "'\n";

	stream << "\n#include \"Object/Class.h\"\n#include \"Object/ObjectMacros.h\"\n\n";

	FString fileId = projectName + "_" + data.FileName + "_h";

	for (auto c : data.classes)
	{
		stream << "#define " << fileId.c_str() << "_" << c.bodyMacro.line << "_GeneratedBody \\\n";
		for (auto p : c.Properties)
			if (p.bPrivate)
				stream << "PRIVATE_MEMBER_OFFSET_ACCESSOR(" << c.name.c_str() << ", " << p.name.c_str() << ") \\\n";

		for (auto f : c.Functions)
		{
			//if (f.bPrivate)
			//{
			//	std::cerr << "error: Function must be public '" << f.name.c_str() << "'\n";
			//	continue;
			//}

			bool bRequiresImpl = f.type != CppFunction::COMMAND && f.type != CppFunction::GENERAL && f.type != CppFunction::OUTPUT;

			stream << "DECLARE_EXEC_FUNCTION(" << f.name.c_str() << ")\\\n";
			
			if (bRequiresImpl)
			{
				stream << "DECLARE_IMPLEMENTATION(" << f.name.c_str();
				for (auto& arg : f.Arguments)
				{
					FString typeName = arg.typeName;
					if (arg.bPointer)
						typeName += '*';
					if (arg.bTemplateType)
						typeName = FString("TObjectPtr<") + arg.typeName + ">";

					stream << ", " << typeName.c_str() << " " << arg.name.c_str();
				}

				stream << ")\\\n";
			}
		}

		if (c.classMacro.type != FMacro::STRUCT)
			stream << "DECLARE_CLASS(" << c.name.c_str() << ", " << c.baseName.c_str() << ", " << projectName.c_str() << ")\n\n";
		else
			stream << "DECLARE_STRUCT(" << c.name.c_str() << ", " << projectName.c_str() << ")\n\n";
	}

	stream << "\n#undef FILE_ID\n#define FILE_ID " << fileId.c_str() << "\n";

	stream.close();
}

static FString GetDisplayName(const FString& in)
{
	if (in.Size() < 2)
		return in;

	FString r = in;
	if (((r[0] == 'b' || r[0] == 'm' || r[0] == 'g' || r[0] == 'C' || r[0] == 'F' || r[0] == 'I') && r[1] < 'a'))
		r.Erase(r.begin());
	else if ((r[0] == 'm' && r[1] == '_'))
		r.Erase(r.begin(), r.begin() + 1);
	else if (r[0] > 'Z')
		r[0] -= 32;

	for (auto it = r.rbegin()++; it != r.rend(); it++)
	{
		if (*it > 'Z' && *(it.ptr + 1) < 'a')
		{
			SizeType ind = r.Index(it);
			r.Insert(' ', ind + 1);
			it = (TReverseIterator<char>)r.At(ind);
		}
	}

	return r;
}

void WriteMetaType(std::ofstream& stream, const FString& metaName, FMacro& macro)
{
	int catI = macro.ArgIndex("Category");

	stream << "#if IS_DEV\n";

	stream << "static TPair<FString, FString> " << metaName.c_str() << "_Tags[]" << "{\n";
	for (auto& arg : macro.Arguments)
		stream << "\t{ \"" << arg.Key.c_str() << "\", \"" << arg.Value.c_str() << "\" },\n";

	stream << "};\n\n";

	stream << "static FPropertyMeta " << metaName.c_str() << " {\n";
	stream << "\t\"" << (catI != -1 ? macro.Arguments[catI].Value.c_str() : "") << "\",\n";

	stream << "\t" << std::to_string(macro.Arguments.Size()) << ",\n";
	stream << "\t" << metaName.c_str() << "_Tags\n";

	stream << "};\n\n";

	stream << "#define " << metaName.c_str() << "_Ptr &" << metaName.c_str() << "\n";
	stream << "#else\n";
	stream << "#define " << metaName.c_str() << "_Ptr nullptr\n";
	stream << "#endif\n";
}

void WriteArgType(std::ofstream& stream, CppProperty& arg, const FString& templateArray = FString())
{
	stream << "{ ";
	FString type = GetVariableType(arg.typeName);
	stream << type.c_str() << ", ";
	stream << "\"" << arg.typeName.c_str() << "\", ";
	if (arg.typeName != "void" && type != "EVT_END")
	{
		if (type != "EVT_CLASS")
			stream << "sizeof(" << GetFullTypename(arg).c_str() << "), ";
		else
			stream << "sizeof(" << GetFullTypename(arg).c_str() << "*), ";
	}
	else
		stream << "0, ";
	stream << arg.templateArgs.Size() << ", ";
	stream << ((arg.templateArgs.Size() > 0) ? templateArray.c_str() : "nullptr") << ", ";
	stream << (arg.bConst ? "true" : "false") << ", ";
	stream << (arg.bRef ? "true" : "false") << ", ";
	stream << (arg.bPointer ? "true" : "false") << " }";
}

void WriteArgTypeTemplateArray(std::ofstream& stream, CppProperty& p, const FString& arrayName, int index = 0, int layer = 0)
{
	// wether this argument has template arguments or not.
	for (int i = 0; i < p.templateArgs.Size(); i++)
	{
		if (p.templateArgs[i].templateArgs.Size() > 0)
			WriteArgTypeTemplateArray(stream, p.templateArgs[i], arrayName, i, layer + 1);
	}

	stream << "static FArgType " << arrayName.c_str();
	//if (index > 0)
	//	stream << "_" << index;
	if (layer > 0)
		stream << "_" << index << "_" << layer;

	stream << "[] = {\n";

	for (int i = 0; i < p.templateArgs.Size(); i++)
	{
		auto& arg = p.templateArgs[i];
		FString templateArray;
		if (arg.templateArgs.Size() > 0)
			templateArray = arrayName + "_" + FString::ToString(i) + "_" + FString::ToString(layer + 1);

		stream << "\t";
		WriteArgType(stream, arg, templateArray);
		stream << ",\n";
	}
	
	stream << "};\n\n";
}

void CParser::WriteGeneratedCpp(const FHeaderData& data)
{
	std::ofstream stream((GeneratedOutput + "/" + data.FileName + ".generated.cpp").c_str(), std::ios::trunc | std::ios::out);
	if (!stream.is_open())
		return;

	FString moduleGetter = FString("GetModule_") + projectName + "()";

	stream << "\n#include <Util/Core.h>\n#include \"" << data.FilePath.c_str() << "\"\n#include \"Object/Class.h\"\n#include \"Module.h\"\n";
	stream << "\n#include \"Object/PropertyTypes.h\"\n";
	//stream << "\nextern CModule " << projectName.c_str() << "_module;\n\n";
	stream << "\nextern CModule& " << moduleGetter.c_str() << ";\n\n";

	for (auto& Enum : data.enums)
	{
		stream	<< "class FEnum_" << Enum.name.c_str() << " : public FEnum\n{\n\tpublic:\n";
		stream << "\tFEnum_" << Enum.name.c_str() << "()\n\t{\n";

		for (auto& var : Enum.Values)
		{
			FString name;
			if (auto i = var.meta.ArgIndex("Name"); i != -1)
				name = var.meta.Arguments[i].Value;
			else
				name = var.name;

			stream << "\t\tvalues.Add({ \"" << name.c_str() << "\", (int64)" << Enum.name.c_str() << "::" << var.name.c_str() << " });\n";
		}

		FString name;
		if (auto i = Enum.macro.ArgIndex("Name"); i != -1)
			name = Enum.macro.Arguments[i].Value;
		else
		{
			name = Enum.name;
			if (name[0] == 'E' && name[1] < 'a')
				name.Erase(name.begin());
		}

		//stream  << "\t\t};\n";
		stream << "\t\tname = \"" << name.c_str() << "\";\n"
			<< "\t\tcppName = \"" << Enum.name.c_str() << "\";\n"
			<< "\t\tsize = sizeof(" << Enum.name.c_str() << ");\n";
			//<< "\t\tflags = EnumFlag_NONE";

		std::string flags = "EnumFlag_NONE";

		for (auto& mf : Enum.macro.Arguments)
		{
			if (mf.Key == "IsFlag" || mf.Key == "Flags")
				flags =  "EnumFlag_IS_FLAG";
		}

		stream << "\t\tflags = " << flags.c_str() << ";\n";

		stream << ";\n";
		stream << "\t\t" << moduleGetter.c_str() << ".RegisterFEnum(this);\n\t}\n};\n";
		stream << "FEnum_" << Enum.name.c_str() << " __FEnum_" << Enum.name.c_str() << "_Instance;\n\n";
	}

	for (auto& Class : data.classes)
	{
		stream << "#undef CLASS_NEXT_PROPERTY\n#define CLASS_NEXT_PROPERTY nullptr\n\n";

		for (auto& p : Class.Properties)
		{
			FString varTypeId = GetVariableType(p.typeName);
			bool bIsEnumInt = p.typeName == "TEnumInt";
			if (varTypeId == "EVT_END")
			{
				std::cerr << "WARNING: invalid property '" << Class.name.c_str() << "::" << p.name.c_str() << "'!\n";
				continue;
			}
			if (!p.IfGuard.IsEmpty())
				stream << "#if " << p.IfGuard.c_str() << "\n";

			if (varTypeId == "EVT_ARRAY")
			{
				FString ArrayTypeName = GetFullTypename(p);
				/*if (p.nestedTemplateType.IsEmpty())
					ArrayTypeName += p.typeName + (p.bPointer ? "*" : "") + ">";
				else
					ArrayTypeName += p.nestedTemplateType + "<" + p.typeName + (p.bPointer ? "*" : "") + ">>";*/

				FString objTypeName = GetFullTypename(p.templateArgs[0]);
				/*if (p.nestedTemplateType.IsEmpty())
					objTypeName = p.typeName + (p.bPointer ? "*" : "");
				else
					objTypeName = p.nestedTemplateType + "<" + p.typeName + (p.bPointer ? "*" : "") + ">";*/

				FString arrayAccessor = "(*(" + ArrayTypeName + "*)ptr)";

				FString type = GetVariableType(p.templateArgs[0].typeName);
				stream << "static FArrayType _arrayType_" << p.name.c_str() << " {\n ";
				stream << "\t[](void* ptr, void* data) { " << arrayAccessor.c_str() << ".Add(*(" << objTypeName.c_str() << "*)data); },\n";		// Add(T&)
				stream << "\t[](void* ptr) { " << arrayAccessor.c_str() << ".Add(); },\n";															// Add()
				stream << "\t[](void* ptr, SizeType i) { " << arrayAccessor.c_str() << ".Erase(" << arrayAccessor.c_str() << ".At(i)); },\n";	// Erase(iterator)
				stream << "\t[](void* ptr) { " << arrayAccessor.c_str() << ".Clear(); },\n";													// Clear()
				stream << "\t[](void* ptr, SizeType i) { " << arrayAccessor.c_str() << ".Resize(i); }, \n";										// Resize(size_t)
				stream << "\t[](void* ptr) { return " << arrayAccessor.c_str() << ".Size(); }, \n";												// Size()
				stream << "\t[](void* ptr) { return " << arrayAccessor.c_str() << ".Capacity(); }, \n";											// Capacity()
				stream << "\t[](void* ptr) { return (void*)" << arrayAccessor.c_str() << ".Data(); }, \n";										// Data()
				stream << "\t[](void* ptr, SizeType i) { return (void*)&*(" << arrayAccessor.c_str() << ".At(i)); } \n";						// At(size_t)
				stream << "};\n\n";
			}

			FString templateArray = "nullptr";
			if (p.templateArgs.Size() > 0 && !bIsEnumInt) // in the event that the type is a TEnumInt we ignore the template arguments since they are only used to determine the enum type and can be ignored.
			{
				templateArray = "_" + Class.name + "_" + p.name + "_Template";
				WriteArgTypeTemplateArray(stream, p, templateArray);
			}

			// MetaData
			bool bHasMeta = false;
			FString metaName = "_" + Class.name + "_" + p.name + "_Meta";
			{
				auto catI = p.macro.ArgIndex("Category");

				bHasMeta = catI != -1 || p.macro.Arguments.Size() != 0;
				if (bHasMeta)
					WriteMetaType(stream, metaName, p.macro);
			}

			FString displayName;
			SizeType dnI = p.macro.ArgIndex("Name");
			if (dnI == -1)
				displayName = GetDisplayName(p.name);
			else
				displayName = p.macro.Arguments[dnI].Value;

			FString tags;
			if (p.bPointer)
				tags += " VTAG_TYPE_POINTER |";
			if (p.bStatic)
				tags += " VTAG_STATIC |";

			for (auto& flag : sVariableFlags)
			{
				SizeType i = p.macro.ArgIndex(flag.first);
				if ((i != -1 && !flag.second.bInverse) || (i == -1 && flag.second.bInverse))
					tags += " " + flag.second.flag + " |";
			}

			if (!tags.IsEmpty())
				tags.Erase(tags.last());
			else
				tags = " VTAG_NONE";

			stream << "DECLARE_PROPERTY(" << Class.name.c_str() << ", \"" 
				<< displayName.c_str() << "\", "
				<< p.name.c_str() << ", \""
				<< p.comment.c_str() << "\", \""
				<< (bIsEnumInt ? p.templateArgs[0].typeName.c_str() : p.typeName.c_str()) << "\", "
				<< varTypeId.c_str() << ","
				<< tags.c_str() << ", ";

			if (p.bPrivate)
				stream << Class.name.c_str() << "::__private_" << p.name.c_str() << "_offset(), ";
			else
				stream << "offsetof(" << Class.name.c_str() << ", " << p.name.c_str() << "), ";
			
			stream << "sizeof(" << GetFullTypename(p).c_str();
			stream << "), ";

			stream << (bHasMeta ? (metaName + "_Ptr, ").c_str() : "nullptr, ");

			if (varTypeId == "EVT_ARRAY")
				stream << "&_arrayType_" << p.name.c_str();
			else
				stream << "nullptr";

			stream << ", " << p.name.Hash() << "ull" // Type ID
				<< ", " << (p.bPrivate ? 0 : 2) // protection lvl
				<< ", " << p.templateArgs.Size()	// number of templates
				<< ", " << templateArray.c_str();	// template array
			
			stream << ")\n";

			stream << "#undef CLASS_NEXT_PROPERTY\n"
				   << "#define CLASS_NEXT_PROPERTY & EVALUATE_PROPERTY_NAME(" << Class.name.c_str() << ", " << p.name.c_str() << ")\n";

			if (!p.IfGuard.IsEmpty())
				stream << "#endif\n";

			stream << "\n";
		}

		if (Class.classMacro.type != FMacro::STRUCT)
			stream << "#undef CLASS_NEXT_FUNCTION\n#define CLASS_NEXT_FUNCTION nullptr\n\n";

		for (auto& f : Class.Functions)
		{
			FString displayName = f.name;
			SizeType dnI = f.macro.ArgIndex("Name");
			if (dnI == -1)
			{
				if (displayName[0] > 'Z')
					displayName[0] -= 32;
			}
			else
				displayName = f.macro.Arguments[dnI].Value;

			FString CmdType = funcCommmandTypes[f.type];

			if (f.Arguments.Size() > 0)
			{
				stream << "static FFuncArg _funcArgs_" << Class.name.c_str() << "_" << f.name.c_str() << "[] = {\n";

				for (auto& arg : f.Arguments)
				{
					FString typeId = GetVariableType(arg.typeName);
					if (typeId == "EVT_END")
					{
						std::cerr << "ERROR: invalid function arg '" << arg.name.c_str() << "'  '" << Class.name.c_str() << "::" << f.name.c_str() << "'!\n";
						continue;
					}

					stream << "\t{ \"" << arg.name.c_str() << "\", ";
					WriteArgType(stream, arg);
					stream << " },\n";
				}

				stream << "};\n";
			}

			// MetaData
			bool bHasMeta = f.macro.Arguments.Size() > 0;
			FString metaName = "_" + Class.name + "_" + f.name + "_Meta";
			if (bHasMeta)
				WriteMetaType(stream, metaName, f.macro);

			FString returnTemplateArr = "nullptr";
			if (f.returnValue.templateArgs.Size() > 0)
			{
				returnTemplateArr = "_" + Class.name + "_" + f.name + "_ReturnValueTemplates";
				WriteArgTypeTemplateArray(stream, f.returnValue, returnTemplateArr);
			}

			FString returnTypeName = "_" + Class.name + "_" + f.name + "_ReturnType";
			stream << "\nstatic FArgType " << returnTypeName.c_str() << " = ";
			WriteArgType(stream, f.returnValue, returnTemplateArr);
			stream << ";\n\n";

			stream << "DECLARE_FUNCTION_PROPERTY("
				<< Class.name.c_str() << ", \""
				<< displayName.c_str() << "\", \""
				<< f.comment.c_str() << "\", "
				<< f.name.c_str() << ", "
				<< f.name.Hash() << "ull, "
				<< (f.bPrivate ? 0 : 2) << ", "
				<< "&" << Class.name.c_str() << "::exec" << f.name.c_str() << ", "
				<< "FFunction::" << CmdType.c_str();
			if (f.Arguments.Size() > 0)
				stream << ", _funcArgs_" << Class.name.c_str() << "_" << f.name.c_str() << ", " << f.Arguments.Size() << ", ";
			else
				stream << ", nullptr, 0, ";

			//WriteArgType(stream, f.returnValue, returnTemplateArr);
			stream << returnTypeName.c_str() << ", ";

			stream << (bHasMeta ? (metaName + "_Ptr, ").c_str() : "nullptr, ");

			FString funcFlags = "FTAG_NONE";
			if (f.macro.ArgIndex("NoEntityInput") == -1)
				funcFlags += " | FTAG_ALLOW_AS_INPUT";

			stream << funcFlags.c_str() << ")\n";
			stream << "#undef CLASS_NEXT_FUNCTION\n" << "#define CLASS_NEXT_FUNCTION &EVALUATE_FUNCTION_NAME(" << Class.name.c_str() << ", " << f.name.c_str() << ")\n\n";
		}

		FString objectTypeName;
		if (Class.classMacro.type == FMacro::CLASS)
			objectTypeName = FString("FClass_") + Class.name;
		else if (Class.classMacro.type == FMacro::STRUCT)
			objectTypeName = FString("FStruct_") + Class.name;
		else
			objectTypeName = FString("FAssetClass_") + Class.name;

		bool bHasTags = Class.classMacro.Arguments.Size() > 0;
		if (bHasTags)
		{
			stream << "#ifdef IS_DEV\n";
			stream << "static TPair<FString, FString> _" << objectTypeName.c_str() << "_Tags[] {\n";
			for (auto& arg : Class.classMacro.Arguments)
				stream << "\t{ \"" << arg.Key.c_str() << "\", \"" << arg.Value.c_str() << "\" },\n";
			stream << "};\n";
			stream << "#endif\n\n";
		}

		if (Class.classMacro.type == FMacro::CLASS)
			stream << "class " << objectTypeName.c_str() << " : public FClass\n{\n";
		else if (Class.classMacro.type == FMacro::STRUCT)
			stream << "class " << objectTypeName.c_str() << " : public FStruct\n{\n";
		else
			stream << "class " << objectTypeName.c_str() << " : public FAssetClass\n{\n";

		FString displayName;
		if (SizeType dnI = Class.classMacro.ArgIndex("Name"); dnI == -1)
			displayName = GetDisplayName(Class.name);
		else
			displayName = Class.classMacro.Arguments[dnI].Value;

		stream << "public:\n\t"
			<< objectTypeName.c_str() << "()\n\t{\n"
			<< "\t\tname = \"" << displayName.c_str() << "\";\n"
			<< "\t\tcppName = \"" << Class.name.c_str() << "\";\n"
			<< "\t\tsize = sizeof(" << Class.name.c_str() << ");\n"
			<< "\t\tnumProperties = " << std::to_string(Class.Properties.Size()) << ";\n"
			<< "\t\tPropertyList = CLASS_NEXT_PROPERTY;\n";
			//<< "\t\tbIsClass = " << (Class.classMacro.type != FMacro::STRUCT ? "true" : "false") << ";\n";

		if (bHasTags)
		{
			stream << "#ifdef IS_DEV\n";
			stream << "\t\tnumTags = " << std::to_string(Class.classMacro.Arguments.Size()) << ";\n";
			stream << "\t\ttags = _" << objectTypeName.c_str() << "_Tags;\n";
			stream << "#endif\n";
		}

		stream << "\t\tconstructor = [](void* obj) { ";

		bool bCanInstantiate = Class.classMacro.ArgIndex("Abstract") == -1;
		if (bCanInstantiate)
			stream << "new(obj) " << Class.name.c_str() << "(); };\n";
		else 
			stream << "};\n";

		// extension is deprecated
		//if (auto i = Class.classMacro.ArgIndex("Extension"); Class.classMacro.type == FMacro::ASSET && Class.classMacro.ArgIndex("Abstract") == -1)
		//{
		//	if (i == -1);
		//		//std::cerr << "error: type of asset must have extension type specified!";
		//	else
		//		stream << "\t\textension = \"" << Class.classMacro.Arguments[i].Value.c_str() << "\";\n";

		//	auto importI = Class.classMacro.ArgIndex("ImportableAs");
		//	if (importI != -1)
		//		stream << "\t\timportableAs = \"" << Class.classMacro.Arguments[importI].Value.c_str() << "\";\n";
		//}

		if (Class.baseName.IsEmpty() || Class.name == "CObject")
			stream << "\t\tbaseType = nullptr;\n";
		else if (Class.classMacro.type == FMacro::STRUCT)
			stream << "\t\tbaseType = " << Class.baseName.c_str() << "::StaticStruct();\n";
		else
			stream << "\t\tbaseType = " << Class.baseName.c_str() << "::StaticClass();\n";

		stream << "\t\tflags =";

		FString flags = " CTAG_NATIVE |";
		if (Class.classMacro.type != FMacro::STRUCT)
			flags += " CTAG_CLASS |";
		if (Class.classMacro.ArgIndex("Abstract") != -1)
			flags += " CTAG_ABSTRACT |";
		if (Class.classMacro.ArgIndex("Hidden") != -1)
			flags += " CTAG_HIDDEN |";

		if (!flags.IsEmpty())
			flags.Erase(flags.last() - 1, flags.end());
		else
			flags = " CTAG_NONE";

		stream << flags.c_str() << ";\n";

		if (Class.classMacro.type != FMacro::STRUCT)
		{
			stream << "\t\tnumFunctions = " << std::to_string(Class.Functions.Size()) << ";\n"
				<< "\t\tFunctionList = CLASS_NEXT_FUNCTION;\n";

			if (Class.classMacro.type == FMacro::ASSET)
			{
				stream << "\t\tassetFlags = ASSET_NONE";
				FString aFlags;
				if (Class.classMacro.ArgIndex("AutoLoad") != -1)
					aFlags += " | ASSET_AUTO_LOAD";

				stream << aFlags.c_str() << ";\n";
			}

			bool bCanInstantiate = Class.classMacro.ArgIndex("Abstract") == -1;
			
			if (Class.classMacro.type == FMacro::CLASS || Class.classMacro.type == FMacro::ASSET)
				stream << "\t\t" << moduleGetter.c_str() << ".RegisterFClass(this);\n";
			if (Class.classMacro.type == FMacro::ASSET)
				stream << "\t\t" << moduleGetter.c_str() << ".RegisterFAsset(this);\n";

			//stream << "\t}\n\tCObject* Instantiate() override { return " << (bCanInstantiate ? (FString("new ") + Class.name + "()").c_str() : "nullptr") << "; }\n};\n";
		}
		else
			stream << "\t\t" << moduleGetter.c_str() << ".RegisterFStruct(this);\n";

		stream << "\t}\n"
			<< "};\n\n";

		stream << objectTypeName.c_str() << " __" << objectTypeName.c_str() << "_Instance;\n";

		if (Class.classMacro.type != FMacro::STRUCT)
			stream << "\nFClass* " << Class.name.c_str() << "::StaticClass() { return &__" << objectTypeName.c_str() << "_Instance; }\n";
		else
			stream << "\nFStruct* " << Class.name.c_str() << "::StaticStruct() { return &__" << objectTypeName.c_str() << "_Instance; }\n";

		stream << "\n#undef CLASS_NEXT_PROPERTY\n#undef CLASS_NEXT_FUNCTION\n";
	}

	for (auto Class : data.classes)
	{
		for (auto& f : Class.Functions)
		{
			bool bIsNetFunc = f.type == CppFunction::SERVER_RPC || f.type == f.type == CppFunction::CLIENT_RPC || f.type == CppFunction::MUTLICAST_RPC;
			bool bHasImpl = bIsNetFunc;

			FString displayName;
			if (SizeType dnI = f.macro.ArgIndex("Name"); dnI == -1)
				displayName = GetDisplayName(f.name);
			else
				displayName = f.macro.Arguments[dnI].Value;

			if (bIsNetFunc)
			{

			}

			if (f.type == CppFunction::OUTPUT)
			{
				stream << "\nvoid " << Class.name.c_str() << "::" << f.name.c_str() << "()\n{\n\tFireOutput(\"" << displayName.c_str() << "\");\n}\n";
			}

			stream << "\nvoid " << Class.name.c_str() << "::exec" << f.name.c_str() << "(CObject* obj, FStack& stack)\n{\n";
			for (auto it = f.Arguments.rbegin(); it != f.Arguments.rend(); it++)
			{
				FString typeName;
				if (it->bTemplateType)
					typeName = it->typeName + "<" + it->templateArgs[0].typeName + ">"; // TODO: support nested templates
				else
					typeName = it->typeName;

				if (it->bPointer)
					typeName += "*";

				stream << "\tPOP_STACK_VARIABLE(" << typeName.c_str() << ", " << it->name.c_str() << ");\n";
			}
			stream << "\t((" << Class.name.c_str() << "*)obj)->" << f.name.c_str();
			if (bHasImpl)
				stream << "_Implementation";
			stream << "(";
			FString finalArgs;
			for (auto& arg : f.Arguments)
				finalArgs += arg.name + ",";
			if (finalArgs.Size() > 0)
				finalArgs.Erase(finalArgs.last());

			stream << finalArgs.c_str() << ");\n}\n";
		}
	}

	stream.close();
}

void CParser::LoadModuleData(const FString& path)
{
	CFStream stream((path + "/module.bin").c_str(), "rb");
	if (!stream.IsOpen())
	{
		std::cerr << "error: failed to open '" << path.c_str() << "Intermediate/module.bin'";
		return;
	}

	FString moduleName;
	stream >> moduleName;

	SizeType numClasses;
	stream >> &numClasses;

	for (SizeType i = 0; i < numClasses; i++)
	{
		FTypeDefinition type;
		type.moduleName = moduleName;
		
		stream >> &type.type;
		stream >> type.name;

		PreRegisteredClasses.Add(type);
	}
}

bool CParser::WriteModuleData()
{
	CFStream stream((targetPath + "/Intermediate/module.bin").c_str(), "wb");
	if (!stream.IsOpen())
		return false;

	SizeType numClasses = 0;
	for (auto& header : Headers)
	{
		numClasses += header.classes.Size();
		numClasses += header.enums.Size();
	}

	stream << projectName;
	stream << &numClasses;

	for (auto& header : Headers)
	{
		for (auto c : header.classes)
		{
			uint8 type = c.classMacro.type != FMacro::STRUCT;
			stream << &type;
			stream << c.name;
		}
		for (auto e : header.enums)
		{
			uint8 type = 2;
			stream << &type;
			stream << e.name;
		}
	}

	return true;
}

bool CParser::HeaderUpToDate(FHeaderData& header)
{
	CFStream stream((GeneratedOutput + "/Timestamp.bin"), "rb");
	if (!stream.IsOpen())
		return false;

	SizeType numFiles = 0;
	stream >> &numFiles;

	for (SizeType i = 0; i < numFiles; i++)
	{
		FString fileName;
		stream >> fileName;

		std::filesystem::file_time_type time;
		stream >> &time;

		if (fileName != header.FileName)
			continue;

		auto nowtime = std::filesystem::last_write_time(header.FilePath.c_str());
		if (nowtime > time)
			return false;

		return true;
	}

	return false;
}

void CParser::WriteTimestamp()
{
	CFStream stream((GeneratedOutput + "/Timestamp.bin"), "wb");
	if (!stream.IsOpen())
		return;

	SizeType numFiles = Headers.Size();
	stream << &numFiles;

	for (auto& h : Headers)
	{
		stream << h.FileName;

		auto time = std::filesystem::last_write_time(h.FilePath.c_str());
		stream << &time;
	}
}
