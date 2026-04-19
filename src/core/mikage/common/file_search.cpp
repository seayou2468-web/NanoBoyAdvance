// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common.h"
#include "common_paths.h"

#include <string>
#include <algorithm>

#include "file_search.h"
#include "string_util.h"


CFileSearch::CFileSearch(const CFileSearch::XStringVector& _rSearchStrings, const CFileSearch::XStringVector& _rDirectories)
{
    // Reverse the loop order for speed?
    for (size_t j = 0; j < _rSearchStrings.size(); j++)
    {
        for (size_t i = 0; i < _rDirectories.size(); i++)
        {
            FindFiles(_rSearchStrings[j], _rDirectories[i]);
        }
    }
}


void CFileSearch::FindFiles(const std::string& _searchString, const std::string& _strPath)
{
    // TODO: super lame/broken

    auto end_match(_searchString);

    // assuming we have a "*.blah"-like pattern
    if (!end_match.empty() && end_match[0] == '*')
        end_match.erase(0, 1);

    // ugly
    if (end_match == ".*")
        end_match.clear();

    DIR* dir = opendir(_strPath.c_str());

    if (!dir)
        return;

    while (auto const dp = readdir(dir))
    {
        std::string found(dp->d_name);

        if ((found != ".") && (found != "..")
            && (found.size() >= end_match.size())
            && std::equal(end_match.rbegin(), end_match.rend(), found.rbegin()))
        {
            std::string full_name;
            if (_strPath.c_str()[_strPath.size()-1] == DIR_SEP_CHR)
                full_name = _strPath + found;
            else
                full_name = _strPath + DIR_SEP + found;

            m_FileNames.push_back(full_name);
        }
    }

    closedir(dir);
}

const CFileSearch::XStringVector& CFileSearch::GetFileNames() const
{
    return m_FileNames;
}
