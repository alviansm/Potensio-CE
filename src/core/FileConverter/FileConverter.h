// src/core/FileConverter/FileConverter.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

enum class FileType
{
    Unknown = 0,
    PNG,
    JPG,
    PDF
};

enum class ConversionType
{
    Compress = 0,
    Convert,
    Both
};

struct FileConversionJob
{
    std::string id;
    std::string inputPath;
    std::string outputPath;
    FileType inputType;
    FileType outputType;
    ConversionType conversionType;
    
    // Compression settings
    int quality = 85; // 0-100 for JPG, 0-9 for PNG
    size_t targetSizeKB = 0; // 0 = no target size
    bool preserveMetadata = false;
    
    // Progress and status
    float progress = 0.0f;
    bool isCompleted = false;
    bool hasError = false;
    std::string errorMessage;
    
    // File info
    size_t originalSizeBytes = 0;
    size_t compressedSizeBytes = 0;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    
    // Helper methods
    std::string GetInputFileName() const;
    std::string GetOutputFileName() const;
    std::string GetFileSizeString(size_t bytes) const;
    float GetCompressionRatio() const;
    std::string GetProcessingTimeString() const;
};

class FileConverter
{
public:
    using ProgressCallback = std::function<void(const std::string& jobId, float progress)>;
    using CompletionCallback = std::function<void(const std::string& jobId, bool success, const std::string& error)>;

public:
    FileConverter();
    ~FileConverter();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Job management
    std::string AddConversionJob(const std::string& inputPath, const std::string& outputPath, 
                            FileType outputType, const FileConversionJob& settings);
    bool RemoveJob(const std::string& jobId);
    void ClearCompleted();
    void ClearAll();
    
    // Processing
    void ProcessJob(const std::string& jobId);
    void ProcessAllJobs();
    void StopProcessing();
    
    // Status and info
    std::vector<std::shared_ptr<FileConversionJob>> GetJobs() const;
    std::shared_ptr<FileConversionJob> GetJob(const std::string& jobId) const;
    bool IsProcessing() const { return m_isProcessing; }
    int GetCompletedJobCount() const;
    int GetFailedJobCount() const;
    
    // Settings
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
    void SetCompletionCallback(CompletionCallback callback) { m_completionCallback = callback; }
    
    // Utility methods
    static FileType GetFileTypeFromPath(const std::string& path);
    static std::string GetFileTypeString(FileType type);
    static bool IsImageFile(FileType type);
    static bool IsPDFFile(FileType type);
    static std::vector<std::string> GetSupportedExtensions();

private:
    std::vector<std::shared_ptr<FileConversionJob>> m_jobs;
    bool m_isProcessing = false;
    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    
    // Processing methods
    bool ProcessImageCompression(std::shared_ptr<FileConversionJob> job);
    bool ProcessImageConversion(std::shared_ptr<FileConversionJob> job);
    bool ProcessPDFCompression(std::shared_ptr<FileConversionJob> job);
    
    // Helper methods
    bool CompressJPEG(const std::string& inputPath, const std::string& outputPath, int quality);
    bool CompressPNG(const std::string& inputPath, const std::string& outputPath, int quality);
    bool ConvertPNGToJPG(const std::string& inputPath, const std::string& outputPath, int quality);
    bool ConvertJPGToPNG(const std::string& inputPath, const std::string& outputPath);
    
    size_t GetFileSize(const std::string& path);
    std::string GenerateJobId();
};