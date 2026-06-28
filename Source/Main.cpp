#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>
#include "SongPlayer.h"
#include "MappedSong.h"

namespace
{
    // Menu options are presented to the user starting at one; the Exit
    // option always follows the last discovered song.
    constexpr int FirstSongOption = 1;

    // Subfolder, beside the executable, that holds the .beep song files.
    constexpr wchar_t SongsFolderName[] = L"Songs";

    constexpr wchar_t SongFileExtension[] = L".beep";

    /// @brief Returns the Songs directory located next to the running
    ///        executable, or an empty path if the executable location
    ///        could not be determined.
    std::filesystem::path GetSongsDirectory()
    {
        wchar_t modulePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        if (0 == length || MAX_PATH == length)
        {
            return std::filesystem::path();
        }

        const std::filesystem::path executablePath(modulePath);
        return executablePath.parent_path() / SongsFolderName;
    }

    /// @brief Maps every valid .beep file in the Songs directory, sorted by
    ///        file name so the menu order is stable across runs.
    std::vector<MappedSong> LoadSongs()
    {
        std::vector<MappedSong> songs;

        const std::filesystem::path songsDir = GetSongsDirectory();

        std::error_code error;
        if (false == std::filesystem::is_directory(songsDir, error))
        {
            return songs;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(songsDir, error))
        {
            if (true == entry.is_regular_file() && SongFileExtension == entry.path().extension())
            {
                files.push_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end());

        songs.reserve(files.size());
        for (const std::filesystem::path& file : files)
        {
            MappedSong song(file.wstring());
            if (true == song.IsValid())
            {
                songs.push_back(std::move(song));
            }
            else
            {
                std::wcout << L"Skipping unreadable song file: "
                    << file.filename().wstring() << std::endl;
            }
        }

        return songs;
    }

    void PrintMenu(const std::vector<MappedSong>& songs, int exitOption)
    {
        std::cout << "===== BeepSongs =====" << std::endl;
        for (size_t i = 0; i < songs.size(); ++i)
        {
            std::cout << (static_cast<int>(i) + FirstSongOption) << ". "
                << songs[i].Name() << std::endl;
        }
        std::cout << exitOption << ". Exit" << std::endl;
        std::cout << "(Ctrl+C to stop playback)" << std::endl;
        std::cout << "Select a song: ";
    }
}

int main()
{
    SetConsoleCtrlHandler(SongPlayer::GetCtrlHandler(), TRUE);

    const std::vector<MappedSong> songs = LoadSongs();
    if (true == songs.empty())
    {
        std::cout << "No songs found. Place .beep files in the Songs folder "
            << "next to the executable." << std::endl;
        return 1;
    }

    const int exitOption = static_cast<int>(songs.size()) + FirstSongOption;

    int selection = 0;
    while (exitOption != selection)
    {
        PrintMenu(songs, exitOption);

        std::cin >> selection;

        if (true == std::cin.eof())
        {
            break;
        }

        if (true == std::cin.fail())
        {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (exitOption == selection)
        {
            std::cout << "Goodbye!" << std::endl;
            continue;
        }

        if (FirstSongOption > selection || selection > exitOption)
        {
            std::cout << "Invalid selection." << std::endl;
            continue;
        }

        const MappedSong& chosen = songs[static_cast<size_t>(selection - FirstSongOption)];
        std::cout << "Playing: " << chosen.Name() << std::endl;
        SongPlayer::PlayTimedSong(chosen.Notes(), chosen.NoteCount());
        std::cout << "Playback complete." << std::endl << std::endl;
    }

    return 0;
}
