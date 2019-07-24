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
			ofs << L"  <file path=\"" << GetActualPathName(file_iter.first) << L"\">" << std::endl;
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
private:
    //Tweaked code from https://stackoverflow.com/a/81493
    //Thanks to cspirz(https://stackoverflow.com/users/8352/cspirz), NeARAZ (https://stackoverflow.com/users/6799/nearaz)
    //Method will change path to case sensitive path ex. C:\test\mycode.cpp to C:\Test\MYCode.cpp
    std::wstring GetActualPathName(std::wstring path)
    {
        const wchar_t kSeparator = L'\\';

        size_t i = 0;

        std::wstring result;
        const auto length = path.size();
        // for network paths (\\server\share\RestOfPath), getting the display
        // name mangles it into unusable form (e.g. "\\server\share" turns
        // into "share on server (server)"). So detect this case and just skip
        // up to two path components
        if (length >= 2 && path[0] == kSeparator && path[1] == kSeparator)
        {
            int skippedCount = 0;
            i = 2; // start after '\\'
            while (i < length && skippedCount < 2)
            {
                if (path[i] == kSeparator)
                    ++skippedCount;
                ++i;
            }

            result.append(path, i);
        }
        // for drive names, just add it uppercased
        else if (length >= 2 && path[1] == L':')
        {
            result += towupper(path[0]);
            result += L':';
            if (length >= 3 && path[2] == kSeparator)
            {
                result += kSeparator;
                i = 3; // start after drive, colon and separator
            }
            else
            {
                i = 2; // start after drive and colon
            }
        }

        size_t lastComponentStart = i;
        bool addSeparator = false;

        while (i < length)
        {
            // skip until path separator
            while (i < length && path[i] != kSeparator)
                ++i;

            if (addSeparator)
                result += kSeparator;

            // if we found path separator, get real filename of this
            // last path name component
            bool foundSeparator = (i < length);
            path[i] = 0;
            SHFILEINFOW info;

            // nuke the path separator so that we get real name of current path component
            info.szDisplayName[0] = 0;
            if (SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info), SHGFI_DISPLAYNAME))
            {
                result += info.szDisplayName;
            }
            else
            {
                // most likely file does not exist.
                // So just append original path name component.
                result.append(path, lastComponentStart, i - lastComponentStart);
            }

            // restore path separator that we might have nuked before
            if (foundSeparator)
                path[i] = kSeparator;

            ++i;
            lastComponentStart = i;
            addSeparator = true;
        }

        return result;
    }
};

extern "C"
{
	//-------------------------------------------------------------------------
	__declspec(dllexport) Plugin::IExportPlugin* CreatePlugin()
	{
		return new SonarQubeExport();
	}
}
