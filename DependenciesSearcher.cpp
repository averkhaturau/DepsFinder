#include "INIReader.h"

#include "TasksPool.h"

#include <array>
#include <iostream>
#include <filesystem>
#include <list>
#include <fstream>
#include <regex>
#include <map>
#include <set>
#include <vector>
#include <future>
#include <algorithm>

using namespace std::filesystem;


struct Params
{
    std::list<path> searchedDirs;
    std::list<path> scannedDirs;
    std::regex searched_extentions_pattern;
    std::regex scanned_extentions_pattern;
};


Params FetchParameters(INIReader& iniReader);

std::list<path> FilterFilesByExtentions(const std::list<path>& sourceDirs, const std::regex& extentionsPattern);

std::map<std::string, std::set<path>> DetectDependencies(const std::list<path>& searched, const std::list<path>& scanned);



int main(int argc, char** argv)
{
    // Проверка аргументов
    if (argc > 2) {
        std::cout << "RTFM!!!" << std::endl;
        return -1;
    }

    INIReader ini((argc == 2) ? argv[1] : "config.ini");

    if (ini.ParseError() < 0)
    {
        std::cout << "Can't load the .ini file";
        return -1;
    }

    // Выдираем параметры из .ini-файла
    Params params = FetchParameters(ini);

    // Формируем списки искомых и сканируемых файлов
    std::list<path> searched_FileNames = FilterFilesByExtentions(params.searchedDirs, params.searched_extentions_pattern);
    std::list<path> scanned_FileNames = FilterFilesByExtentions(params.scannedDirs, params.scanned_extentions_pattern);

    std::cout << "Searching dependencies of ";
    for (const auto& dir : params.searchedDirs)
        std::cout << dir << " ";
    std::cout << " in ";
    for (const auto& dir : params.scannedDirs)
        std::cout << dir << " ";
    std::cout << " (" << scanned_FileNames.size() << " files)\n";

    const auto start = std::chrono::steady_clock::now();

    // Сканируем файлы на присутствие имён искомых файлов
    const auto potentialDependencies = DetectDependencies(searched_FileNames, scanned_FileNames);

    const auto finish = std::chrono::steady_clock::now();
    std::cout << "\nWorked " << std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count() << "ms\n";

    std::cout << "Writing results to dependencies.txt\n";
    // Выводим лог зависимостей
    std::ofstream results("dependencies.txt");
    for (const auto& dep : potentialDependencies)
    {
        results << "Name of the file \"" << dep.first << "\" is present in file(s):";
        for (const auto& where_ : dep.second)
        {
            results << "\n\t\"" << where_.generic_string();
        }
        results << std::endl;
    }

    return 0;
}



Params FetchParameters(INIReader& iniReader)
{
    Params params;
    params.searchedDirs = iniReader.GetPathList("PATHS", "Searched");
    params.scannedDirs = iniReader.GetPathList("PATHS", "Scanned");

    std::list<std::string> searched_extentions = iniReader.GetStringList("EXTENTIONS", "Searched");
    std::list<std::string> scanned_extentions = iniReader.GetStringList("EXTENTIONS", "Scanned");

    std::string searched_regStr = "(";
    std::string scanned_regStr = "(";

    for (const auto& str : searched_extentions) {
        searched_regStr += "\\" + str + "|";
    }
    for (const auto& str : scanned_extentions) {
        scanned_regStr += "\\" + str + "|";
    }
    searched_regStr.back() = ')';
    scanned_regStr.back() = ')';

    params.searched_extentions_pattern = std::regex(searched_regStr);
    params.scanned_extentions_pattern = std::regex(scanned_regStr);

    return params;
}


std::list<path> FilterFilesByExtentions(const std::list<path>& sourceDirs, const std::regex& extentionsPattern)
{
    std::list<path> filtered_files;
    for (const auto& sourceDir : sourceDirs)
    {
        if (!std::filesystem::exists(sourceDir))
        {
            std::cout << "Path doesn't exist:\n\t" << sourceDir << "\n";
            continue;
        }

        recursive_directory_iterator source_it(sourceDir);
        for (; source_it != recursive_directory_iterator(); ++source_it)
        {
            if (source_it->is_directory())
                continue;

            if (std::regex_match(source_it->path().extension().string(), extentionsPattern))
            {
                filtered_files.push_back(source_it->path());
            }
        }
    }
    return filtered_files;
}


// Возвращает unordered_map< 
//                          key: искомый файл,
//                          value: список файлов, в которых присутствует его имя
//                         >
std::map<std::string, std::set<path>> DetectDependencies(const std::list<path>& searched_FileNames, const std::list<path>& scanned_FileNames)
{
    std::map<std::string, std::set<path>> potentialDependencies;

    std::cout << "[0%] preparing...\r";
    {
        std::mutex mut_writeDependency;
        TasksPool todo;

        size_t i = 0;
        for (const auto& scanned_FileName : scanned_FileNames) {
            if (++i % ((scanned_FileNames.size()/5)+1) == 0)
                std::cout << "[" << i * 5 / scanned_FileNames.size() << "%] searching...  \r";
            todo.addTask([scanned_FileName, &searched_FileNames, &mut_writeDependency, &potentialDependencies]()
                {
                    std::string scanned_FileContent;
                    {
                        std::ifstream scanned_File(scanned_FileName);

                        if (!scanned_File.is_open()) {
                            std::cout << "Cannot open " << scanned_FileName << "\n";
                            return;
                        }
                        scanned_FileContent = std::string((std::istreambuf_iterator<char>(scanned_File)), std::istreambuf_iterator<char>());
                    }

                    for (const auto& searched_FileName : searched_FileNames)
                    {
                        std::regex pattern_toSearch(searched_FileName.filename().string());
                        if (std::regex_search(scanned_FileContent, pattern_toSearch))
                        {
                            std::lock_guard<std::mutex> lock(mut_writeDependency);
                            potentialDependencies[searched_FileName.generic_string()].insert(scanned_FileName);
                        }
                    }
                });
        }

        // reporting percentage
        for (short percentage = 0; percentage < 90; percentage = todo.progress()) {
            std::cout << "["<< 5 + percentage <<"%] searching...\r";
            std::this_thread::sleep_for(std::chrono::seconds(4));
        }

        std::cout << "[98%] searching...        \r";
    }
    std::cout << "[100%] done.             \n";

    return potentialDependencies;
}
