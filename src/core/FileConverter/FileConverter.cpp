// src/core/FileConverter/FileConverter.cpp
#include "FileConverter.h"
#include "core/Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <thread>
#include <chrono>

// FileConversionJob helper methods implementation
std::string FileConversionJob::GetInputFileName() const
{
    std::filesystem::path p(inputPath);
    return p.filename().string();
}

std::string FileConversionJob::GetOutputFileName() const
{
    std::filesystem::path p(outputPath);
    return p.filename().string();
}

std::string FileConversionJob::GetFileSizeString(size_t bytes) const
{
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 3)
    {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    return oss.str();
}

float FileConversionJob::GetCompressionRatio() const
{
    if (originalSizeBytes == 0) return 0.0f;
    if (compressedSizeBytes >= originalSizeBytes) return 0.0f;
    
    return ((static_cast<float>(originalSizeBytes - compressedSizeBytes) / originalSizeBytes) * 100.0f);
}

std::string FileConversionJob::GetProcessingTimeString() const
{
    if (startTime == std::chrono::system_clock::time_point{} || 
        endTime == std::chrono::system_clock::time_point{})
    {
        return "Unknown";
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    auto ms = duration.count();
    
    if (ms < 1000)
    {
        return std::to_string(ms) + "ms";
    }
    else
    {
        return std::to_string(ms / 1000) + "." + std::to_string((ms % 1000) / 100) + "s";
    }
}

// FileConverter implementation
FileConverter::FileConverter()
{
}

FileConverter::~FileConverter()
{
    Shutdown();
}

bool FileConverter::Initialize()
{
    Logger::Info("FileConverter initialized");
    return true;
}

void FileConverter::Shutdown()
{
    StopProcessing();
    m_jobs.clear();
    Logger::Debug("FileConverter shutdown complete");
}

std::string FileConverter::AddConversionJob(const std::string& inputPath, const std::string& outputPath, 
                                           FileType outputType, const FileConversionJob& settings)
{
    auto job = std::make_shared<FileConversionJob>(settings);
    job->id = GenerateJobId();
    job->inputPath = inputPath;
    job->outputPath = outputPath;
    job->inputType = GetFileTypeFromPath(inputPath);
    job->outputType = outputType;
    
    // Determine conversion type
    if (job->inputType == job->outputType)
    {
        job->conversionType = ConversionType::Compress;
    }
    else
    {
        job->conversionType = ConversionType::Convert;
    }
    
    // Get original file size
    job->originalSizeBytes = GetFileSize(inputPath);
    
    m_jobs.push_back(job);
    
    Logger::Debug("Added conversion job: {} -> {}", inputPath, outputPath);
    return job->id;
}

bool FileConverter::RemoveJob(const std::string& jobId)
{
    auto it = std::find_if(m_jobs.begin(), m_jobs.end(),
        [&jobId](const std::shared_ptr<FileConversionJob>& job) {
            return job->id == jobId;
        });
    
    if (it != m_jobs.end())
    {
        m_jobs.erase(it);
        Logger::Debug("Removed job: {}", jobId);
        return true;
    }
    
    return false;
}

void FileConverter::ClearCompleted()
{
    auto newEnd = std::remove_if(m_jobs.begin(), m_jobs.end(),
        [](const std::shared_ptr<FileConversionJob>& job) {
            return job->isCompleted;
        });
    
    m_jobs.erase(newEnd, m_jobs.end());
    Logger::Debug("Cleared completed jobs");
}

void FileConverter::ClearAll()
{
    m_jobs.clear();
    Logger::Debug("Cleared all jobs");
}

void FileConverter::ProcessJob(const std::string& jobId)
{
    auto job = GetJob(jobId);
    if (!job)
    {
        Logger::Warning("Job not found: {}", jobId);
        return;
    }
    
    if (job->isCompleted)
    {
        Logger::Debug("Job already completed: {}", jobId);
        return;
    }
    
    m_isProcessing = true;
    job->startTime = std::chrono::system_clock::now();
    job->progress = 0.1f;
    
    if (m_progressCallback)
        m_progressCallback(jobId, job->progress);
    
    bool success = false;
    std::string errorMessage;
    
    try
    {
        // Simulate processing steps
        job->progress = 0.3f;
        if (m_progressCallback) m_progressCallback(jobId, job->progress);
        
        if (IsImageFile(job->inputType))
        {
            if (job->conversionType == ConversionType::Compress)
            {
                success = ProcessImageCompression(job);
            }
            else
            {
                success = ProcessImageConversion(job);
            }
        }
        else if (IsPDFFile(job->inputType))
        {
            success = ProcessPDFCompression(job);
        }
        else
        {
            errorMessage = "Unsupported file type";
        }
        
        if (success)
        {
            job->compressedSizeBytes = GetFileSize(job->outputPath);
        }
    }
    catch (const std::exception& e)
    {
        errorMessage = e.what();
        Logger::Error("Job processing failed: {}", errorMessage);
    }
    
    job->endTime = std::chrono::system_clock::now();
    job->progress = 1.0f;
    job->isCompleted = true;
    job->hasError = !success;
    
    if (!success && !errorMessage.empty())
    {
        job->errorMessage = errorMessage;
    }
    
    m_isProcessing = false;
    
    if (m_progressCallback)
        m_progressCallback(jobId, job->progress);
    
    if (m_completionCallback)
        m_completionCallback(jobId, success, errorMessage);
    
    Logger::Debug("Job {} {}", jobId, success ? "completed successfully" : "failed");
}

void FileConverter::ProcessAllJobs()
{
    for (const auto& job : m_jobs)
    {
        if (!job->isCompleted)
        {
            ProcessJob(job->id);
        }
    }
}

void FileConverter::StopProcessing()
{
    m_isProcessing = false;
}

std::vector<std::shared_ptr<FileConversionJob>> FileConverter::GetJobs() const
{
    return m_jobs;
}

std::shared_ptr<FileConversionJob> FileConverter::GetJob(const std::string& jobId) const
{
    auto it = std::find_if(m_jobs.begin(), m_jobs.end(),
        [&jobId](const std::shared_ptr<FileConversionJob>& job) {
            return job->id == jobId;
        });
    
    return (it != m_jobs.end()) ? *it : nullptr;
}

int FileConverter::GetCompletedJobCount() const
{
    return static_cast<int>(std::count_if(m_jobs.begin(), m_jobs.end(),
        [](const std::shared_ptr<FileConversionJob>& job) {
            return job->isCompleted && !job->hasError;
        }));
}

int FileConverter::GetFailedJobCount() const
{
    return static_cast<int>(std::count_if(m_jobs.begin(), m_jobs.end(),
        [](const std::shared_ptr<FileConversionJob>& job) {
            return job->isCompleted && job->hasError;
        }));
}

// Static utility methods
FileType FileConverter::GetFileTypeFromPath(const std::string& path)
{
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".png") return FileType::PNG;
    if (ext == ".jpg" || ext == ".jpeg") return FileType::JPG;
    if (ext == ".pdf") return FileType::PDF;
    
    return FileType::Unknown;
}

std::string FileConverter::GetFileTypeString(FileType type)
{
    switch (type)
    {
        case FileType::PNG: return "PNG";
        case FileType::JPG: return "JPG";
        case FileType::PDF: return "PDF";
        default: return "Unknown";
    }
}

bool FileConverter::IsImageFile(FileType type)
{
    return type == FileType::PNG || type == FileType::JPG;
}

bool FileConverter::IsPDFFile(FileType type)
{
    return type == FileType::PDF;
}

std::vector<std::string> FileConverter::GetSupportedExtensions()
{
    return {".png", ".jpg", ".jpeg", ".pdf"};
}

// Private helper methods
bool FileConverter::ProcessImageCompression(std::shared_ptr<FileConversionJob> job)
{
    job->progress = 0.5f;
    if (m_progressCallback) m_progressCallback(job->id, job->progress);
    
    // Simulate compression
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (job->inputType == FileType::JPG)
    {
        return CompressJPEG(job->inputPath, job->outputPath, job->quality);
    }
    else if (job->inputType == FileType::PNG)
    {
        return CompressPNG(job->inputPath, job->outputPath, job->quality);
    }
    
    return false;
}

bool FileConverter::ProcessImageConversion(std::shared_ptr<FileConversionJob> job)
{
    job->progress = 0.5f;
    if (m_progressCallback) m_progressCallback(job->id, job->progress);
    
    // Simulate conversion
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    
    if (job->inputType == FileType::PNG && job->outputType == FileType::JPG)
    {
        return ConvertPNGToJPG(job->inputPath, job->outputPath, job->quality);
    }
    else if (job->inputType == FileType::JPG && job->outputType == FileType::PNG)
    {
        return ConvertJPGToPNG(job->inputPath, job->outputPath);
    }
    
    return false;
}

bool FileConverter::ProcessPDFCompression(std::shared_ptr<FileConversionJob> job)
{
    job->progress = 0.5f;
    if (m_progressCallback) m_progressCallback(job->id, job->progress);
    
    // Simulate PDF compression - this is a placeholder
    // In a real implementation, you'd use a PDF library like PDFium or MuPDF
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // For now, just copy the file as a placeholder
    try
    {
        std::filesystem::copy_file(job->inputPath, job->outputPath, 
                                  std::filesystem::copy_options::overwrite_existing);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("PDF compression failed: {}", e.what());
        return false;
    }
}

bool FileConverter::CompressJPEG(const std::string& inputPath, const std::string& outputPath, int quality)
{
    // Placeholder implementation - in a real app you'd use a library like libjpeg
    try
    {
        std::filesystem::copy_file(inputPath, outputPath, 
                                  std::filesystem::copy_options::overwrite_existing);
        Logger::Debug("JPEG compression simulated: {} -> {}", inputPath, outputPath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("JPEG compression failed: {}", e.what());
        return false;
    }
}

bool FileConverter::CompressPNG(const std::string& inputPath, const std::string& outputPath, int quality)
{
    // Placeholder implementation - in a real app you'd use a library like libpng
    try
    {
        std::filesystem::copy_file(inputPath, outputPath, 
                                  std::filesystem::copy_options::overwrite_existing);
        Logger::Debug("PNG compression simulated: {} -> {}", inputPath, outputPath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("PNG compression failed: {}", e.what());
        return false;
    }
}

bool FileConverter::ConvertPNGToJPG(const std::string& inputPath, const std::string& outputPath, int quality)
{
    // Placeholder implementation - in a real app you'd use image processing libraries
    try
    {
        std::filesystem::copy_file(inputPath, outputPath, 
                                  std::filesystem::copy_options::overwrite_existing);
        Logger::Debug("PNG to JPG conversion simulated: {} -> {}", inputPath, outputPath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("PNG to JPG conversion failed: {}", e.what());
        return false;
    }
}

bool FileConverter::ConvertJPGToPNG(const std::string& inputPath, const std::string& outputPath)
{
    // Placeholder implementation
    try
    {
        std::filesystem::copy_file(inputPath, outputPath, 
                                  std::filesystem::copy_options::overwrite_existing);
        Logger::Debug("JPG to PNG conversion simulated: {} -> {}", inputPath, outputPath);
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::Error("JPG to PNG conversion failed: {}", e.what());
        return false;
    }
}

size_t FileConverter::GetFileSize(const std::string& path)
{
    try
    {
        return std::filesystem::file_size(path);
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

std::string FileConverter::GenerateJobId()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    return "job_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()) + "_" + std::to_string(dis(gen));
}