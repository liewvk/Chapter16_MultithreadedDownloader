#define NOMINMAX
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>
#include <limits>
#include <filesystem>
#include <curl/curl.h>

struct DownloadTask
{
    std::string url;
    std::string outputFilename;
};

std::mutex consoleMutex;

void safePrint(const std::string& message)
{
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::cout << message << std::endl;
}

size_t writeFileCallback(void* contents, size_t size, size_t nmemb, void* userData)
{
    size_t totalSize = size * nmemb;

    std::ofstream* file = static_cast<std::ofstream*>(userData);

    if (file && file->is_open())
    {
        file->write(static_cast<char*>(contents), totalSize);
    }

    return totalSize;
}

bool createDownloadsFolder()
{
    try
    {
        std::filesystem::create_directories("downloads");
        return true;
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cout << "Error creating downloads folder: " << e.what() << std::endl;
        return false;
    }
}

void downloadFile(const DownloadTask& task, int threadNumber)
{
    std::ostringstream startMessage;
    startMessage << "[Thread " << threadNumber << "] Downloading "
        << task.outputFilename;
    safePrint(startMessage.str());

    if (!createDownloadsFolder())
    {
        std::ostringstream errorMessage;
        errorMessage << "[Thread " << threadNumber << "] Error: Could not create downloads folder.";
        safePrint(errorMessage.str());
        return;
    }

    CURL* curl = curl_easy_init();

    if (!curl)
    {
        std::ostringstream errorMessage;
        errorMessage << "[Thread " << threadNumber << "] Error: Could not initialize CURL.";
        safePrint(errorMessage.str());
        return;
    }

    std::ofstream outputFile(task.outputFilename, std::ios::binary);

    if (!outputFile.is_open())
    {
        std::ostringstream errorMessage;
        errorMessage << "[Thread " << threadNumber << "] Error: Could not open output file "
            << task.outputFilename;
        safePrint(errorMessage.str());

        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, task.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outputFile);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode result = curl_easy_perform(curl);

    outputFile.close();

    if (result == CURLE_OK)
    {
        std::ostringstream successMessage;
        successMessage << "[Thread " << threadNumber << "] Completed: "
            << task.outputFilename;
        safePrint(successMessage.str());
    }
    else
    {
        std::ostringstream errorMessage;
        errorMessage << "[Thread " << threadNumber << "] Download failed: "
            << curl_easy_strerror(result);
        safePrint(errorMessage.str());
    }

    curl_easy_cleanup(curl);
}

void displayMenu()
{
    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "      Multithreaded File Downloader" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "1. Add Download URL" << std::endl;
    std::cout << "2. View Download Queue" << std::endl;
    std::cout << "3. Start Downloads" << std::endl;
    std::cout << "4. Clear Download Queue" << std::endl;
    std::cout << "5. Exit" << std::endl;
    std::cout << "Please choose an option: ";
}

void addDownloadTask(std::vector<DownloadTask>& downloadQueue)
{
    DownloadTask task;

    std::cout << "Enter file URL: ";
    std::getline(std::cin, task.url);

    std::cout << "Enter output filename, for example sample.txt: ";
    std::getline(std::cin, task.outputFilename);

    if (task.url.empty())
    {
        std::cout << "URL cannot be empty." << std::endl;
        return;
    }

    if (task.outputFilename.empty())
    {
        std::cout << "Output filename cannot be empty." << std::endl;
        return;
    }

    std::filesystem::path filenameOnly = std::filesystem::path(task.outputFilename).filename();

    task.outputFilename = (std::filesystem::path("downloads") / filenameOnly).string();

    downloadQueue.push_back(task);

    std::cout << "Download added to queue." << std::endl;
}

void viewDownloadQueue(const std::vector<DownloadTask>& downloadQueue)
{
    if (downloadQueue.empty())
    {
        std::cout << "Download queue is empty." << std::endl;
        return;
    }

    std::cout << std::endl;
    std::cout << "========== Download Queue ==========" << std::endl;

    for (int i = 0; i < static_cast<int>(downloadQueue.size()); ++i)
    {
        std::cout << i + 1 << ". URL: "
            << downloadQueue[i].url << std::endl;

        std::cout << "   Output: "
            << downloadQueue[i].outputFilename << std::endl;
    }
}

void startDownloads(const std::vector<DownloadTask>& downloadQueue)
{
    if (downloadQueue.empty())
    {
        std::cout << "Download queue is empty." << std::endl;
        return;
    }

    if (!createDownloadsFolder())
    {
        std::cout << "Downloads cannot start because the downloads folder could not be created." << std::endl;
        return;
    }

    std::vector<std::thread> threads;

    std::cout << "Starting downloads..." << std::endl;

    for (int i = 0; i < static_cast<int>(downloadQueue.size()); ++i)
    {
        threads.emplace_back(downloadFile, downloadQueue[i], i + 1);
    }

    for (std::thread& worker : threads)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    std::cout << "All downloads completed." << std::endl;
}

void clearDownloadQueue(std::vector<DownloadTask>& downloadQueue)
{
    downloadQueue.clear();

    std::cout << "Download queue cleared." << std::endl;
}

int main()
{
    CURLcode initResult = curl_global_init(CURL_GLOBAL_DEFAULT);

    if (initResult != CURLE_OK)
    {
        std::cout << "Error: Could not initialize libcurl." << std::endl;
        return 1;
    }

    createDownloadsFolder();

    std::vector<DownloadTask> downloadQueue;
    int choice;

    while (true)
    {
        displayMenu();

        if (!(std::cin >> choice))
        {
            std::cout << "Invalid input. Please enter a number." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice)
        {
        case 1:
            addDownloadTask(downloadQueue);
            break;

        case 2:
            viewDownloadQueue(downloadQueue);
            break;

        case 3:
            startDownloads(downloadQueue);
            break;

        case 4:
            clearDownloadQueue(downloadQueue);
            break;

        case 5:
            curl_global_cleanup();
            std::cout << "Thank you for using the Multithreaded File Downloader." << std::endl;
            return 0;

        default:
            std::cout << "Invalid option. Please choose again." << std::endl;
        }
    }
}