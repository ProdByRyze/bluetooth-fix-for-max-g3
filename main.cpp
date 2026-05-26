#define NOMINMAX
#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// Neutral SHU Bluetooth Fix Tool
// Windows console tool for private/test use.
// It does not flash anything. It only creates a patched copy of
// a full 128KB MEMORY_G3.bin dump.
// ============================================================

static const size_t EXPECTED_DUMP_SIZE = 131072;
static const std::string MARKER = "SCOOTER_VCU_xxG3";

// ============================================================
// Console colors
// ============================================================

enum class Color : WORD
{
    Default = 7,
    Gray = 8,
    Blue = 9,
    Green = 10,
    Aqua = 11,
    Red = 12,
    Purple = 13,
    Yellow = 14,
    White = 15
};

void setColor(Color color)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), static_cast<WORD>(color));
}

void printColor(const std::string& text, Color color, bool newline = true)
{
    setColor(color);
    std::cout << text;
    if (newline)
        std::cout << "\n";
    setColor(Color::Default);
}

void printSection(const std::string& title)
{
    std::cout << "\n";
    printColor("============================================================", Color::Aqua);
    printColor("  " + title, Color::White);
    printColor("============================================================", Color::Aqua);
}

void printSuccess(const std::string& text)
{
    printColor("[OK] " + text, Color::Green);
}

void printInfo(const std::string& text)
{
    printColor("[INFO] " + text, Color::Blue);
}

void printWarn(const std::string& text)
{
    printColor("[WARNING] " + text, Color::Yellow);
}

void printError(const std::string& text)
{
    printColor("[ERROR] " + text, Color::Red);
}

void printAction(const std::string& text)
{
    printColor("[ACTION] " + text, Color::Purple);
}

void clearConsole()
{
    system("cls");
}

void waitForEnterToMenu()
{
    std::cout << "\n";
    printColor("Press ENTER to return to main menu...", Color::Gray);
    std::string dummy;
    std::getline(std::cin, dummy);
}

void configureConsole()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleA("G3 SHU Bluetooth Fix Tool");

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hConsole == INVALID_HANDLE_VALUE)
        return;

    const SHORT cols = 140;
    const SHORT rows = 42;
    const SHORT bufferRows = 2500;

    SMALL_RECT tiny{};
    tiny.Left = 0;
    tiny.Top = 0;
    tiny.Right = 80;
    tiny.Bottom = 25;
    SetConsoleWindowInfo(hConsole, TRUE, &tiny);

    COORD buffer{};
    buffer.X = cols;
    buffer.Y = bufferRows;
    SetConsoleScreenBufferSize(hConsole, buffer);

    SMALL_RECT rect{};
    rect.Left = 0;
    rect.Top = 0;
    rect.Right = cols - 1;
    rect.Bottom = rows - 1;
    SetConsoleWindowInfo(hConsole, TRUE, &rect);
}

void printWatermark()
{
    setColor(Color::Purple);

    std::cout << R"(



  /$$$$$$  /$$   /$$ /$$$$$$$$  /$$$$$$ 
 /$$__  $$| $$  | $$|____ /$$/ /$$__  $$
| $$  \__/| $$  | $$   /$$$$/ | $$$$$$$$
| $$      | $$  | $$  /$$__/  | $$_____/
| $$      |  $$$$$$$ /$$$$$$$$|  $$$$$$$
|__/       \____  $$|________/ \_______/
           /$$  | $$                    
          |  $$$$$$/                    
           \______/                     

)";

    setColor(Color::Default);

    printColor("G3 SHU BLUETOOTH FIX TOOL", Color::White);
    printColor("Credits to sharkboy for the method on how to fix the bluetooth problem.\n", Color::Gray);
}

// ============================================================
// Paths / helpers
// ============================================================

fs::path getExecutableDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);

    if (length == 0 || length == MAX_PATH)
        return fs::current_path();

    fs::path exePath(buffer);
    return exePath.parent_path();
}

fs::path getBackupsDirectory()
{
    return getExecutableDirectory() / "backups";
}

fs::path getLogsDirectory()
{
    return getExecutableDirectory() / "logs";
}

std::string nowTimestamp()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << st.wYear
        << std::setw(2) << st.wMonth
        << std::setw(2) << st.wDay
        << "_"
        << std::setw(2) << st.wHour
        << std::setw(2) << st.wMinute
        << std::setw(2) << st.wSecond;

    return oss.str();
}

std::string trim(const std::string& input)
{
    size_t start = 0;

    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
        start++;

    size_t end = input.size();

    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
        end--;

    return input.substr(start, end - start);
}

std::string toLower(std::string input)
{
    for (char& c : input)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return input;
}

std::string hexOffset(size_t value)
{
    std::ostringstream oss;

    oss << "0x"
        << std::uppercase
        << std::hex
        << std::setw(6)
        << std::setfill('0')
        << value;

    return oss.str();
}

std::string hexSize(size_t value)
{
    std::ostringstream oss;

    oss << "0x"
        << std::uppercase
        << std::hex
        << value;

    return oss.str();
}

bool askYesNo(const std::string& question, bool defaultYes)
{
    while (true)
    {
        setColor(Color::Yellow);
        std::cout << question << (defaultYes ? " (Y/n): " : " (y/N): ");
        setColor(Color::Default);

        std::string input;
        std::getline(std::cin, input);
        input = toLower(trim(input));

        if (input.empty())
            return defaultYes;

        if (input == "y" || input == "yes" || input == "j" || input == "ja")
            return true;

        if (input == "n" || input == "no" || input == "nein")
            return false;

        printWarn("Please enter Y or N.");
    }
}

bool confirmExact(const std::string& expected)
{
    std::cout << "\n";
    printColor("Safety confirmation required.", Color::Yellow);
    std::cout << "Type exactly ";
    printColor("\"" + expected + "\"", Color::Purple, false);
    std::cout << " to continue: ";

    std::string input;
    std::getline(std::cin, input);

    return input == expected;
}

bool fileExists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

void ensureDir(const fs::path& path)
{
    std::error_code ec;
    fs::create_directories(path, ec);
}

void openFolderInExplorer(const fs::path& folder)
{
    ensureDir(folder);

    std::wstring cmd = L"explorer \"" + folder.wstring() + L"\"";
    _wsystem(cmd.c_str());
}

std::vector<uint8_t> readFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return {};

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0)
        return {};

    std::vector<uint8_t> data(static_cast<size_t>(size));

    file.read(reinterpret_cast<char*>(data.data()), size);

    if (!file)
        return {};

    return data;
}

bool writeFile(const fs::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary);

    if (!file)
        return false;

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    return static_cast<bool>(file);
}

uint32_t crc32Buffer(const uint8_t* data, size_t length, uint32_t previous = 0xFFFFFFFF)
{
    static uint32_t table[256];
    static bool initialized = false;

    if (!initialized)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;

            for (int j = 0; j < 8; ++j)
            {
                if (c & 1)
                    c = 0xEDB88320u ^ (c >> 1);
                else
                    c >>= 1;
            }

            table[i] = c;
        }

        initialized = true;
    }

    uint32_t crc = previous;

    for (size_t i = 0; i < length; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);

    return crc;
}

uint32_t crc32(const std::vector<uint8_t>& data)
{
    if (data.empty())
        return 0;

    return crc32Buffer(data.data(), data.size()) ^ 0xFFFFFFFF;
}

std::string crcHex(uint32_t value)
{
    std::ostringstream oss;

    oss << std::uppercase
        << std::hex
        << std::setw(8)
        << std::setfill('0')
        << value;

    return oss.str();
}

// ============================================================
// Windows file picker
// ============================================================

fs::path openFilePicker()
{
    wchar_t fileName[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetConsoleWindow();
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter =
        L"VCU Dump Files (*.bin)\0*.bin\0"
        L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Select your full 128KB MEMORY_G3.bin dump";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn))
        return fs::path(fileName);

    return {};
}

fs::path saveFilePicker(const fs::path& defaultPath)
{
    wchar_t fileName[MAX_PATH] = L"";

    std::wstring defaultString = defaultPath.wstring();
    wcsncpy_s(fileName, defaultString.c_str(), MAX_PATH - 1);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetConsoleWindow();
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter =
        L"BIN Files (*.bin)\0*.bin\0"
        L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Choose output file for patched SHU Bluetooth fix dump";
    ofn.lpstrDefExt = L"bin";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn))
        return fs::path(fileName);

    return defaultPath;
}

// ============================================================
// Search / patch logic
// ============================================================

bool isKeyChar(uint8_t byte)
{
    return std::isalnum(static_cast<unsigned char>(byte)) != 0;
}

std::string bytesToAscii(const std::vector<uint8_t>& data, size_t offset, size_t length)
{
    std::string output;

    for (size_t i = 0; i < length && offset + i < data.size(); ++i)
        output.push_back(static_cast<char>(data[offset + i]));

    return output;
}

std::vector<uint8_t> asciiToBytes(const std::string& value)
{
    return std::vector<uint8_t>(value.begin(), value.end());
}

std::vector<size_t> findAllBytes(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern)
{
    std::vector<size_t> hits;

    if (pattern.empty() || data.size() < pattern.size())
        return hits;

    for (size_t i = 0; i <= data.size() - pattern.size(); ++i)
    {
        bool match = true;

        for (size_t j = 0; j < pattern.size(); ++j)
        {
            if (data[i + j] != pattern[j])
            {
                match = false;
                break;
            }
        }

        if (match)
            hits.push_back(i);
    }

    return hits;
}

std::vector<size_t> findAllAscii(const std::vector<uint8_t>& data, const std::string& value)
{
    return findAllBytes(data, asciiToBytes(value));
}

bool allFF(const std::vector<uint8_t>& data, size_t offset, size_t length)
{
    if (offset + length > data.size())
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        if (data[offset + i] != 0xFF)
            return false;
    }

    return true;
}

struct Candidate
{
    std::string value;
    std::vector<size_t> offsets;
};

std::vector<std::string> extractKeyLikeTokensNearMarker(
    const std::vector<uint8_t>& data,
    size_t markerOffset
)
{
    std::vector<std::string> tokens;

    size_t scanStart = markerOffset + MARKER.size();
    size_t scanEnd = std::min(markerOffset + 0x160, data.size());

    size_t i = scanStart;

    while (i < scanEnd)
    {
        while (i < scanEnd && !isKeyChar(data[i]))
            i++;

        size_t tokenStart = i;

        while (i < scanEnd && isKeyChar(data[i]))
            i++;

        size_t tokenLength = i - tokenStart;

        if (tokenLength >= 16 && tokenLength <= 64)
        {
            std::string token = bytesToAscii(data, tokenStart, tokenLength);

            bool hasLetter = false;
            bool hasDigit = false;

            for (char c : token)
            {
                if (std::isalpha(static_cast<unsigned char>(c)))
                    hasLetter = true;

                if (std::isdigit(static_cast<unsigned char>(c)))
                    hasDigit = true;
            }

            if (hasLetter && hasDigit)
                tokens.push_back(token);
        }
    }

    return tokens;
}

std::vector<Candidate> detectDynamicKeyCandidates(const std::vector<uint8_t>& data)
{
    std::map<std::string, std::vector<size_t>> candidateMap;

    std::vector<size_t> markerHits = findAllAscii(data, MARKER);

    for (size_t markerOffset : markerHits)
    {
        std::vector<std::string> tokens = extractKeyLikeTokensNearMarker(data, markerOffset);

        for (const std::string& token : tokens)
        {
            std::vector<size_t> hits = findAllAscii(data, token);

            if (hits.size() == 2)
                candidateMap[token] = hits;
        }
    }

    std::vector<Candidate> candidates;

    for (const auto& item : candidateMap)
    {
        Candidate candidate;
        candidate.value = item.first;
        candidate.offsets = item.second;
        candidates.push_back(candidate);
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
        {
            if (a.offsets.empty() || b.offsets.empty())
                return a.value < b.value;

            return a.offsets[0] < b.offsets[0];
        });

    return candidates;
}

// ============================================================
// Diff / report
// ============================================================

struct DiffRange
{
    size_t start = 0;
    size_t length = 0;
};

std::vector<DiffRange> buildDiffRanges(const std::vector<uint8_t>& original, const std::vector<uint8_t>& patched)
{
    std::vector<DiffRange> ranges;

    if (original.size() != patched.size())
        return ranges;

    size_t i = 0;

    while (i < original.size())
    {
        if (original[i] == patched[i])
        {
            i++;
            continue;
        }

        size_t start = i;

        while (i < original.size() && original[i] != patched[i])
            i++;

        ranges.push_back({ start, i - start });
    }

    return ranges;
}

bool verifyOnlyExpectedChanges(
    const std::vector<uint8_t>& original,
    const std::vector<uint8_t>& patched,
    const Candidate& candidate
)
{
    std::vector<DiffRange> diffs = buildDiffRanges(original, patched);

    if (diffs.size() != candidate.offsets.size())
        return false;

    std::vector<size_t> sortedOffsets = candidate.offsets;
    std::sort(sortedOffsets.begin(), sortedOffsets.end());

    std::sort(diffs.begin(), diffs.end(), [](const DiffRange& a, const DiffRange& b)
        {
            return a.start < b.start;
        });

    for (size_t i = 0; i < diffs.size(); ++i)
    {
        if (diffs[i].start != sortedOffsets[i])
            return false;

        if (diffs[i].length != candidate.value.size())
            return false;

        if (!allFF(patched, diffs[i].start, diffs[i].length))
            return false;
    }

    return true;
}

bool backupOriginal(const fs::path& inputPath, fs::path& backupPathOut)
{
    ensureDir(getBackupsDirectory());

    backupPathOut = getBackupsDirectory() / ("MEMORY_G3_before_shu_bluetooth_fix_" + nowTimestamp() + ".bin");

    std::error_code ec;
    fs::copy_file(inputPath, backupPathOut, fs::copy_options::overwrite_existing, ec);

    return !ec;
}

bool writeReport(
    const fs::path& reportPath,
    const fs::path& inputPath,
    const fs::path& outputPath,
    const Candidate& candidate,
    const std::vector<uint8_t>& original,
    const std::vector<uint8_t>& patched
)
{
    std::ofstream report(reportPath);

    if (!report)
        return false;

    report << "G3 SHU Bluetooth Fix Report\n";
    report << "Timestamp: " << nowTimestamp() << "\n\n";

    report << "Credits: sharkboy method for SHU Bluetooth fix\n\n";

    report << "Input:  " << inputPath.string() << "\n";
    report << "Output: " << outputPath.string() << "\n\n";

    report << "Original size: " << original.size() << " bytes\n";
    report << "Patched size:  " << patched.size() << " bytes\n\n";

    report << "Original CRC32: " << crcHex(crc32(original)) << "\n";
    report << "Patched CRC32:  " << crcHex(crc32(patched)) << "\n\n";

    report << "Detected dynamic key: " << candidate.value << "\n";
    report << "Key length: " << candidate.value.size() << " bytes\n";
    report << "Occurrences patched: " << candidate.offsets.size() << "\n";

    for (size_t offset : candidate.offsets)
        report << " - " << hexOffset(offset) << "\n";

    report << "\nDiff ranges:\n";

    std::vector<DiffRange> diffs = buildDiffRanges(original, patched);

    for (const DiffRange& diff : diffs)
    {
        report << " - Start: " << hexOffset(diff.start)
            << ", length: 0x"
            << std::uppercase
            << std::hex
            << diff.length
            << std::dec
            << " (" << diff.length << " bytes)\n";
    }

    return true;
}

Candidate chooseCandidate(const std::vector<Candidate>& candidates)
{
    if (candidates.empty())
        return {};

    printSection("DETECTED KEY CANDIDATES");

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        setColor(Color::Purple);
        std::cout << "[" << (i + 1) << "] ";
        setColor(Color::White);

        std::cout << "\"" << candidates[i].value << "\"";

        setColor(Color::Gray);
        std::cout << " | length: " << candidates[i].value.size()
            << " | occurrences: " << candidates[i].offsets.size()
            << "\n";
        setColor(Color::Default);

        for (size_t offset : candidates[i].offsets)
        {
            std::cout << "    ";
            printColor("- " + hexOffset(offset), Color::Blue);
        }
    }

    if (candidates.size() == 1)
    {
        if (askYesNo("\nUse this candidate?", true))
            return candidates[0];

        return {};
    }

    std::cout << "\nSelect candidate number, or 0 to abort: ";

    std::string input;
    std::getline(std::cin, input);

    int index = 0;

    try
    {
        index = std::stoi(input);
    }
    catch (...)
    {
        index = 0;
    }

    if (index < 1 || static_cast<size_t>(index) > candidates.size())
        return {};

    return candidates[static_cast<size_t>(index - 1)];
}

// ============================================================
// Patch workflow
// ============================================================

bool runPatchWorkflow()
{
    printSection("IMPORTANT SAFETY INFO");
    printWarn("This tool does NOT flash anything.");
    printWarn("It only creates a patched 128KB dump copy.");
    printWarn("It does NOT overwrite your original dump.");
    printWarn("It only patches if the detected key appears exactly 2 times.");
    printWarn("Verify the output in HxD before using it with any flashing script.");
    printWarn("Private/testing use only.");

    printSection("SELECT INPUT FILE");
    printInfo("A file picker will open now.");
    printInfo("Select your full 128KB MEMORY_G3.bin dump.");

    fs::path inputPath = openFilePicker();

    if (inputPath.empty())
    {
        printError("No input file selected.");
        return false;
    }

    if (!fileExists(inputPath))
    {
        printError("Selected input file does not exist.");
        std::cout << inputPath.string() << "\n";
        return false;
    }

    std::vector<uint8_t> original = readFile(inputPath);

    if (original.empty())
    {
        printError("Could not read selected input file.");
        return false;
    }

    printSection("INPUT FILE LOADED");
    printColor("Path:  " + inputPath.string(), Color::Blue);
    std::cout << "Size:  " << original.size() << " bytes\n";
    std::cout << "CRC32: " << crcHex(crc32(original)) << "\n";

    if (original.size() != EXPECTED_DUMP_SIZE)
    {
        printError("File is not exactly 131072 bytes / 128KB.");
        printError("This is not a full MEMORY_G3.bin dump. Aborting.");
        return false;
    }

    printSuccess("File size is exactly 128KB.");

    printSection("MARKER SEARCH");

    std::vector<size_t> markerHits = findAllAscii(original, MARKER);

    std::cout << "Marker: ";
    printColor("\"" + MARKER + "\"", Color::Purple, false);
    std::cout << "\nFound: " << markerHits.size() << " time(s)\n";

    for (size_t markerOffset : markerHits)
        printColor(" - " + hexOffset(markerOffset), Color::Blue);

    if (markerHits.empty())
    {
        printError("Marker was not found. The dump layout may be different.");
        return false;
    }

    printSection("DYNAMIC KEY DETECTION");

    std::vector<Candidate> candidates = detectDynamicKeyCandidates(original);
    Candidate selected = chooseCandidate(candidates);

    if (selected.value.empty())
    {
        printError("No valid key candidate selected.");
        return false;
    }

    selected.offsets = findAllAscii(original, selected.value);

    printSection("SELECTED KEY");
    std::cout << "Value:       ";
    printColor(selected.value, Color::Purple);
    std::cout << "Length:      " << selected.value.size() << " bytes\n";
    std::cout << "Occurrences: " << selected.offsets.size() << "\n";

    for (size_t offset : selected.offsets)
        printColor(" - " + hexOffset(offset), Color::Blue);

    if (selected.offsets.size() != 2)
    {
        printError("The selected key does not appear exactly 2 times.");
        printError("Expected behavior: the key appears exactly twice in the dump.");
        return false;
    }

    if (selected.value.size() < 16 || selected.value.size() > 64)
    {
        printError("Key length looks unsafe/unexpected. Aborting.");
        return false;
    }

    printSection("PATCH PLAN");
    printAction("The following ranges will be replaced with FF:");

    for (size_t offset : selected.offsets)
    {
        std::cout << " - Start: ";
        printColor(hexOffset(offset), Color::Blue, false);
        std::cout << ", length: ";
        printColor(hexSize(selected.value.size()), Color::Purple, false);
        std::cout << " (" << selected.value.size() << " bytes)\n";
    }

    fs::path defaultOutputPath = inputPath.parent_path() / "MEMORY_G3_shu_bluetooth_fix.bin";
    fs::path outputPath = defaultOutputPath;

    if (askYesNo("\nChoose output file with save dialog?", false))
    {
        outputPath = saveFilePicker(defaultOutputPath);
    }

    printColor("\nOutput file:", Color::Aqua);
    printColor(outputPath.string(), Color::Blue);

    printSection("FINAL CONFIRMATION");
    printWarn("A backup will be created first.");
    printWarn("The original input file will not be modified.");
    printWarn("The tool will only write the new patched output file.");

    if (!confirmExact("PATCH"))
    {
        printWarn("Aborted. No files were changed.");
        return false;
    }

    fs::path backupPath;

    if (!backupOriginal(inputPath, backupPath))
    {
        printError("Could not create backup. Aborting.");
        return false;
    }

    printSection("BACKUP CREATED");
    printColor(backupPath.string(), Color::Green);

    std::vector<uint8_t> patched = original;

    for (size_t offset : selected.offsets)
    {
        for (size_t i = 0; i < selected.value.size(); ++i)
            patched[offset + i] = 0xFF;
    }

    if (patched.size() != EXPECTED_DUMP_SIZE)
    {
        printError("Internal error: patched output size changed.");
        return false;
    }

    if (!verifyOnlyExpectedChanges(original, patched, selected))
    {
        printError("Diff verification failed.");
        printError("The output does not only contain the expected FF ranges. Aborting.");
        return false;
    }

    if (!writeFile(outputPath, patched))
    {
        printError("Could not write output file.");
        return false;
    }

    ensureDir(getLogsDirectory());

    fs::path reportPath = getLogsDirectory() / ("shu_bluetooth_fix_report_" + nowTimestamp() + ".txt");
    writeReport(reportPath, inputPath, outputPath, selected, original, patched);

    printSection("PATCH CREATED SUCCESSFULLY");
    printSuccess("Patched output file written.");
    printColor("Output: " + outputPath.string(), Color::Blue);
    printColor("Report: " + reportPath.string(), Color::Blue);

    std::cout << "\nOriginal CRC32: ";
    printColor(crcHex(crc32(original)), Color::White);
    std::cout << "Patched CRC32:  ";
    printColor(crcHex(crc32(patched)), Color::White);

    std::vector<size_t> remainingHits = findAllAscii(patched, selected.value);

    if (remainingHits.empty())
    {
        printSuccess("The detected key is no longer present in the patched output.");
    }
    else
    {
        printError("The detected key is still present after patching.");

        for (size_t offset : remainingHits)
            printColor(" - " + hexOffset(offset), Color::Red);
    }

    printSection("DIFF VERIFICATION");

    std::vector<DiffRange> diffs = buildDiffRanges(original, patched);

    for (const DiffRange& diff : diffs)
    {
        std::cout << " - Start: ";
        printColor(hexOffset(diff.start), Color::Blue, false);
        std::cout << ", length: ";
        printColor(std::to_string(diff.length) + " bytes", Color::Purple, false);
        std::cout << " -> FF\n";
    }

    printSection("NEXT STEPS");
    printWarn("1. Open the output in HxD.");
    printWarn("2. Confirm both key locations are FF.");
    printWarn("3. Confirm the file is still exactly 131072 bytes.");
    printWarn("4. Keep your original dump backup safe.");
    printWarn("5. Only then consider copying it to MEMORY_G3.bin.patched.bin for the flashing script.");

    fs::path patchedTargetPath = inputPath.parent_path() / "MEMORY_G3.bin.patched.bin";

    if (askYesNo("\nCopy output to MEMORY_G3.bin.patched.bin next to the selected dump?", false))
    {
        std::error_code ec;
        fs::copy_file(outputPath, patchedTargetPath, fs::copy_options::overwrite_existing, ec);

        if (ec)
        {
            printError("Could not copy to MEMORY_G3.bin.patched.bin.");
            std::cout << ec.message() << "\n";
            return false;
        }

        printSuccess("Copied to:");
        printColor(patchedTargetPath.string(), Color::Green);
    }
    else
    {
        printWarn("Not copied to patched flash file. Safer this way.");
    }

    printSection("DONE");
    printSuccess("The tool did NOT flash anything.");
    printSuccess("It only created the patched dump file.");

    return true;
}

// ============================================================
// Menu
// ============================================================

void printMainMenu()
{
    printSection("MAIN MENU");
    std::cout << "[1] Patch MEMORY_G3.bin for SHU Bluetooth fix\n";
    std::cout << "[2] Open backups folder\n";
    std::cout << "[3] Open logs folder\n";
    std::cout << "[0] Exit\n";
    printColor("============================================================", Color::Aqua);
    std::cout << "Selection: ";
}

int main()
{
    configureConsole();

    ensureDir(getBackupsDirectory());
    ensureDir(getLogsDirectory());

    while (true)
    {
        clearConsole();
        printWatermark();
        printMainMenu();

        std::string choice;
        std::getline(std::cin, choice);
        choice = trim(choice);

        if (choice == "0")
        {
            clearConsole();
            printWatermark();
            printColor("Goodbye.", Color::Aqua);
            return 0;
        }

        if (choice == "1")
        {
            clearConsole();
            printWatermark();

            bool ok = runPatchWorkflow();

            if (!ok)
            {
                std::cout << "\n";
                printWarn("Operation stopped or failed. No flashing was performed.");
            }

            waitForEnterToMenu();
            continue;
        }

        if (choice == "2")
        {
            openFolderInExplorer(getBackupsDirectory());
            continue;
        }

        if (choice == "3")
        {
            openFolderInExplorer(getLogsDirectory());
            continue;
        }

        printWarn("Invalid selection.");
        waitForEnterToMenu();
    }
}
