#include "stdafx.h"

#include "Plugin/Exporter/IExportPlugin.hpp"
#include "Plugin/Exporter/CoverageData.hpp"
#include "Plugin/Exporter/ModuleCoverage.hpp"
#include "Plugin/Exporter/FileCoverage.hpp"
#include "Plugin/Exporter/LineCoverage.hpp"
#include "Plugin/OptionsParserException.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <map>
#include <string>


class SonarQubeExport : public Plugin::IExportPlugin
{
public:
	//-------------------------------------------------------------------------
	std::optional<std::filesystem::path> Export(
		const Plugin::CoverageData& coverageData,
		const std::optional<std::wstring>& argument ) override
	{
		std::wstring currentfile;
		std::filesystem::path output = argument ? *argument : L"SonarQube.xml";
		std::wofstream ofs{ output };

		if ( !ofs )
			throw std::runtime_error( "Cannot create the output file for SonarQube exporting" );

		// Convert to our internal maps. We do this because we want to collate the same file if it is seen multiple times
		for ( const auto& mod : coverageData.GetModules() )
		{
			// Skip the module if it has no files
			if ( mod->GetFiles().empty() )
				continue;

			for ( const auto& file : mod->GetFiles() )
			{
				// Skip the file if it has no lines
				if ( file->GetLines().empty() )
					continue;

				currentfile = file->GetPath().wstring();
				auto &file_entry = coverage[currentfile];

				for ( const auto &line : file->GetLines() )
				{
					// In case we have seen thils file/line before, we want to 'or' in the iteration's HasBeenExecuted
					auto &line_entry = file_entry[line.GetLineNumber()];
					line_entry |= line.HasBeenExecuted();
				}
			}
		}

		ofs << L"<coverage version=\"1\">" << std::endl;

		std::wstring covered;
		for ( const auto &file_iter : coverage )
		{
			ofs << L"  <file path=\"" << file_iter.first << L"\">" << std::endl;
			for ( const auto &line_iter : file_iter.second )
			{
				covered = line_iter.second ? L"true" : L"false";
				ofs << L"    <lineToCover lineNumber=\"" << line_iter.first << L"\" covered=\"" << covered << L"\"/>" << std::endl;
			}
			ofs << L"  </file>" << std::endl;
		}
		ofs << L"</coverage>" << std::endl;

		return output;
	}

	//-------------------------------------------------------------------------
	void CheckArgument( const std::optional<std::wstring>& argument ) override
	{
		// Try to check if the argument is a file.
		if ( argument && !std::filesystem::path{ *argument }.has_filename() )
			throw Plugin::OptionsParserException( "Invalid argument for SonarQube export." );
	}

	//-------------------------------------------------------------------------
	std::wstring GetArgumentHelpDescription() override
	{
		return L"output file (optional)";
	}

	//-------------------------------------------------------------------------
	int GetExportPluginVersion() const override
	{
		return Plugin::CurrentExportPluginVersion;
	}

protected:
	std::unordered_map< std::wstring, std::map<size_t, bool> > coverage;
};

extern "C"
{
	//-------------------------------------------------------------------------
	__declspec(dllexport) Plugin::IExportPlugin* CreatePlugin()
	{
		return new SonarQubeExport();
	}
}
