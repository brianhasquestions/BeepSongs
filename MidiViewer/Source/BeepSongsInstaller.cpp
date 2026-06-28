#include "BeepSongsInstaller.h"

#include <Windows.h>
#include <memory>
#include <string>
#include <vector>

#include "BeepWriter.h"
#include "NoteConverter.h"
#include "NoteFilters.h"

namespace
{
    constexpr size_t MaxPathChars = 1024;
    constexpr int MaxParentWalkDepth = 8;
    const char* const VcxprojFileName = "BeepSongs.vcxproj";
    const char* const SongsRelative = "Songs";
    const char* const SongFileExtension = ".beep";

    std::unique_ptr<std::string> GetExecutableDir()
    {
        auto pBuf = std::make_unique<std::vector<wchar_t>>(MaxPathChars, L'\0');
        const DWORD len = GetModuleFileNameW(nullptr, pBuf->data(), static_cast<DWORD>(pBuf->size()));
        if (0 == len)
        {
            return std::make_unique<std::string>();
        }

        std::wstring widePath(pBuf->data(), len);
        const size_t slash = widePath.find_last_of(L"\\/");
        if (std::wstring::npos != slash)
        {
            widePath.resize(slash);
        }

        const int needed = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        auto pOut = std::make_unique<std::string>();
        if (1 < needed)
        {
            pOut->resize(static_cast<size_t>(needed - 1));
            WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, &(*pOut)[0], needed, nullptr, nullptr);
        }

        return pOut;
    }

    bool FileExists(const std::string& path)
    {
        const DWORD attrs = GetFileAttributesA(path.c_str());
        return (INVALID_FILE_ATTRIBUTES != attrs && 0 == (attrs & FILE_ATTRIBUTE_DIRECTORY));
    }

    std::unique_ptr<std::vector<ConvertedNote>> CollectCheckedNotesFrom(const AppState* pState)
    {
        return NoteFilters::BuildMergedNotes(pState, false);
    }
}

namespace BeepSongsInstaller
{
    std::unique_ptr<std::string> FindBeepSongsRoot()
    {
        auto pCurrent = GetExecutableDir();
        if (true == pCurrent->empty())
        {
            return std::make_unique<std::string>();
        }

        std::string current = *pCurrent;
        for (int depth = 0; depth < MaxParentWalkDepth; ++depth)
        {
            const std::string probe = current + "\\" + VcxprojFileName;
            if (true == FileExists(probe))
            {
                return std::make_unique<std::string>(current);
            }
            const size_t slash = current.find_last_of("\\/");
            if (std::string::npos == slash)
            {
                break;
            }
            current.resize(slash);
        }

        return std::make_unique<std::string>();
    }

    std::unique_ptr<std::string> Install(const AppState* pState, const ExportDialog::ExportInfo& info)
    {
        auto pResult = std::make_unique<std::string>();

        auto pRoot = FindBeepSongsRoot();
        if (true == pRoot->empty())
        {
            *pResult = "ERROR: Could not locate BeepSongs.vcxproj by walking up from the executable directory.";
            return pResult;
        }
        const std::string& root = *pRoot;

        auto pNotes = CollectCheckedNotesFrom(pState);
        if (true == pNotes->empty())
        {
            *pResult = "ERROR: No checked tracks contain any notes.";
            return pResult;
        }

        // The Songs folder holds the .beep assets; the BeepSongs runtime
        // discovers them at launch, so writing one file is all it takes.
        const std::string songsDir = root + "\\" + SongsRelative;
        CreateDirectoryA(songsDir.c_str(), nullptr);

        const std::string filePath = songsDir + "\\" + info.BaseName + SongFileExtension;

        BeepWriter::SongInfo songInfo;
        songInfo.DisplayName = info.DisplayName.empty() ? info.BaseName : info.DisplayName;
        songInfo.BpmEstimate = info.BpmEstimate;

        if (false == BeepWriter::Write(*pNotes, songInfo, filePath))
        {
            *pResult = "ERROR: Failed to write " + filePath;
            return pResult;
        }

        *pResult = "OK: Wrote " + filePath + " (" + std::to_string(pNotes->size()) + " notes). It is bundled into BeepSongs on the next build.";
        return pResult;
    }
}
