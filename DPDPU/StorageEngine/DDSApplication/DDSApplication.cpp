#include <atomic>
#include <DDSFrontEnd.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <Windows.h>

#include <chrono>
using namespace std;
using namespace std::chrono;

#include "Profiler.h"

#define PAGE_SIZE 8192

using namespace std;

std::mutex mtx;
std::condition_variable cv;
HANDLE eventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);

void
DummyCallback(
    DDS_FrontEnd::ErrorCodeT ErrorCode,
    DDS_FrontEnd::FileIOSizeT BytesServiced,
    DDS_FrontEnd::ContextT Context
) {
    size_t* completedOperations = (size_t*)Context;
    (*completedOperations)++;
}

void BenchmarkIO(
    DDS_FrontEnd::DDSFrontEnd &Store,
    DDS_FrontEnd::FileIdT FileId,
    DDS_FrontEnd::FileSizeT MaxFileSize
) {
    char* writeBuffer = new char[PAGE_SIZE];
    memset(writeBuffer, 0, sizeof(char) * PAGE_SIZE);
    *((int*)writeBuffer) = 42;
    DDS_FrontEnd::FileSizeT totalWrittenSize = 0;
    const size_t totalIOs = MaxFileSize / PAGE_SIZE;
    size_t completedIOs = 0;
    DDS_FrontEnd::FileIOSizeT bytesServiced;

    DDS_FrontEnd::ErrorCodeT result = Store.SetFilePointer(
        FileId,
        0,
        DDS_FrontEnd::FilePointerPosition::BEGIN
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to set file pointer" << endl;
    }
    else {
        cout << "File pointer reset" << endl;
    }

    cout << "Benchmarking writes..." << endl;
    Profiler profiler(totalIOs);
    profiler.Start();
    for (size_t i = 0; i != totalIOs; i++) {
        result = Store.WriteFile(
            FileId,
            writeBuffer,
            PAGE_SIZE,
            &bytesServiced,
            DummyCallback,
            &completedIOs
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            cout << "Failed to write file: " << FileId << endl;
        }
    }

    while (completedIOs != totalIOs) {}
    profiler.Stop();
    profiler.Report();

    result = Store.SetFilePointer(
        FileId,
        0,
        DDS_FrontEnd::FilePointerPosition::BEGIN
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to set file pointer" << endl;
    }
    else {
        cout << "File pointer reset" << endl;
    }

    cout << "Benchmarking reads..." << endl;
    char* readBuffer = new char[PAGE_SIZE * DDS_FRONTEND_MAX_OUTSTANDING];
    memset(readBuffer, 0, sizeof(char) * PAGE_SIZE * DDS_FRONTEND_MAX_OUTSTANDING);
    completedIOs = 0;
    profiler.Start();
    for (size_t i = 0; i != totalIOs; i++) {
        result = Store.ReadFile(
            FileId,
            &readBuffer[(i % DDS_FRONTEND_MAX_OUTSTANDING) * PAGE_SIZE],
            PAGE_SIZE,
            &bytesServiced,
            DummyCallback,
            &completedIOs
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            cout << "Failed to read file: " << FileId << endl;
        }
    }

    while (completedIOs != totalIOs) {}
    profiler.Stop();
    profiler.Report();
}

void 
PollThread(
    DDS_FrontEnd::DDSFrontEnd *Store,
    DDS_FrontEnd::FileIdT FileId,
    size_t NumOps
) {
    size_t completedOps = 0;
    size_t totalBytes = 0;
    DDS_FrontEnd::PollIdT pollId;
    DDS_FrontEnd::ErrorCodeT result;

    result = Store->GetDefaultPoll(&pollId);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to get default poll" << endl;
        return;
    }

    DDS_FrontEnd::FileIOSizeT opSize;
    DDS_FrontEnd::ContextT opContext;
    DDS_FrontEnd::ContextT pollContext;
    bool pollResult;
    while (completedOps != NumOps) {
        result = Store->PollWait(
            pollId,
            &opSize,
            &pollContext,
            &opContext,
            0,
            &pollResult
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            cout << "Failed to poll an operation" << endl;
            return;
        }

        if (pollResult) {
            totalBytes += opSize;
            completedOps++;
        }
    }
}

void 
BenchmarkIOWithPolling(
    DDS_FrontEnd::DDSFrontEnd& Store,
    DDS_FrontEnd::FileIdT FileId,
    DDS_FrontEnd::FileSizeT MaxFileSize
) {
    char* writeBuffer = new char[PAGE_SIZE];
    memset(writeBuffer, 0, sizeof(char) * PAGE_SIZE);
    *((int*)writeBuffer) = 42;
    DDS_FrontEnd::FileSizeT totalWrittenSize = 0;
    const size_t totalIOs = MaxFileSize / PAGE_SIZE;
    DDS_FrontEnd::FileIOSizeT bytesServiced;

    DDS_FrontEnd::ErrorCodeT result = Store.SetFilePointer(
        FileId,
        0,
        DDS_FrontEnd::FilePointerPosition::BEGIN
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to set file pointer" << endl;
    }
    else {
        cout << "File pointer reset" << endl;
    }

    cout << "Benchmarking writes..." << endl;
    Profiler profiler(totalIOs);
    profiler.Start();

    thread* pollWorker = new thread(
        [&Store, FileId, totalIOs]
        { PollThread(&Store, FileId, totalIOs);}
    );

    for (size_t i = 0; i != totalIOs; i++) {
        result = Store.WriteFile(
            FileId,
            writeBuffer,
            PAGE_SIZE,
            &bytesServiced,
            NULL,
            NULL
        );

        while (result == DDS_ERROR_CODE_REQUIRE_POLLING) {
            result = Store.WriteFile(
                FileId,
                writeBuffer,
                PAGE_SIZE,
                &bytesServiced,
                NULL,
                NULL
            );
        }

        if (result != DDS_ERROR_CODE_IO_PENDING) {
            cout << "Failed to write file: " << FileId << endl;
        }
    }

    pollWorker->join();
    
    profiler.Stop();
    profiler.Report();

    result = Store.SetFilePointer(
        FileId,
        0,
        DDS_FrontEnd::FilePointerPosition::BEGIN
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to set file pointer" << endl;
    }
    else {
        cout << "File pointer reset" << endl;
    }

    cout << "Benchmarking reads..." << endl;
    char* readBuffer = new char[PAGE_SIZE * DDS_FRONTEND_MAX_OUTSTANDING];
    memset(readBuffer, 0, sizeof(char) * PAGE_SIZE * DDS_FRONTEND_MAX_OUTSTANDING);
    
    pollWorker = new thread(
        [&Store, FileId, totalIOs]
        { PollThread(&Store, FileId, totalIOs); }
    );

    profiler.Start();
    for (size_t i = 0; i != totalIOs; i++) {
        result = Store.ReadFile(
            FileId,
            &readBuffer[(i % DDS_FRONTEND_MAX_OUTSTANDING) * PAGE_SIZE],
            PAGE_SIZE,
            &bytesServiced,
            NULL,
            NULL
        );

        while (result == DDS_ERROR_CODE_REQUIRE_POLLING) {
            result = Store.ReadFile(
                FileId,
                writeBuffer,
                PAGE_SIZE,
                &bytesServiced,
                NULL,
                NULL
            );
        }

        if (result != DDS_ERROR_CODE_IO_PENDING) {
            cout << "Failed to read file: " << FileId << endl;
        }
    }
    pollWorker->join();
    
    profiler.Stop();
    profiler.Report();

    delete pollWorker;
}

DWORD WINAPI SleepThread(LPVOID lpParam) {
    SetEvent(eventHandle);
    Sleep(30000);
    return 0;
}

int main()
{
    // atomic test
    std::atomic<int> ai = 0;
    cout << "Size of atomic<int> is " << sizeof(ai) << endl;

    return 0;

    // Mutex test

    // std::thread t = std::thread(SleepThread);
    CreateThread(NULL, 0, SleepThread, NULL, 0, NULL);

    // cout << "Waiting for mutex..." << endl;
    std::unique_lock<std::mutex> lck(mtx);
    auto prevClock = high_resolution_clock::now();
    
    WaitForSingleObject(eventHandle, INFINITE);

    auto nextClock = high_resolution_clock::now();
    cout << "Event got in " << (nextClock - prevClock).count() << " ns" << endl;

    // Waiting test
    static HANDLE timer = CreateWaitableTimer(NULL, FALSE, NULL);
    LARGE_INTEGER due;
    due.QuadPart = -10LL; // *100 ns
    while (true) {
        // double sleepSecs = 0.00001;
        auto prevClock = high_resolution_clock::now();
        // this_thread::sleep_for(nanoseconds((int64_t)(sleepSecs * 1e9)));

        BOOL bSuccess = SetWaitableTimerEx(timer, &due, 0, NULL, NULL, NULL, 0);
        WaitForSingleObject(timer, INFINITE);
        
        auto nextClock = high_resolution_clock::now();
        double deltaTime = (nextClock - prevClock).count() / 1e9;
        // printf("Latency: %.6lf ms\n", deltaTime * 1e3);
        prevClock = nextClock;
    }
    return 0;

    const char* storeName = "DDS-Store0";
    const char* rootDirName = "/data";
    const char* fileName = "/data/example";
    const DDS_FrontEnd::FileSizeT maxFileSize = 10737418240ULL;
    const DDS_FrontEnd::FileAccessT fileAccess = 0;
    const DDS_FrontEnd::FileShareModeT shareMode = 0;
    const DDS_FrontEnd::FileAttributesT fileAttributes = 0;
    DDS_FrontEnd::DirIdT rootDirId;
    DDS_FrontEnd::FileIdT fileId;

    DDS_FrontEnd::DDSFrontEnd store(storeName);

    DDS_FrontEnd::ErrorCodeT result = store.Initialize();
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to initialize DDS front end" << endl;
    }
    else {
        cout << "DDS front end initialized " << endl;
    }

    // result = store.CreateDirectory(rootDirName, &rootDirId);
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to create directory" << endl;
    }
    else {
        cout << "Directory created: " << rootDirId << endl;
    }

    // result = store.CreateFile(
    //    fileName,
    //    fileAccess,
    //    shareMode,
    //    fileAttributes,
    //    &fileId
    // );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to create file" << endl;
    }
    else {
        cout << "File created: " << fileId << endl;
    }

    int dataToWrite = 42;
    int readBuffer = 0;
    size_t ioCount = 0;
    DDS_FrontEnd::FileIOSizeT bytesServiced = 0;

    result = store.WriteFile(
        fileId,
        &dataToWrite,
        sizeof(int),
        &bytesServiced,
        DummyCallback,
        &ioCount
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to write file" << endl;
    }
    else {
        cout << "Write posted" << endl;
    }

    while (ioCount != 1) {
        this_thread::yield();
    }

    cout << "Data has been written" << endl;

    result = store.SetFilePointer(
        fileId,
        0,
        DDS_FrontEnd::FilePointerPosition::BEGIN
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to set file pointer" << endl;
    }
    else {
        cout << "File pointer reset" << endl;
    }

    result = store.ReadFile(
        fileId,
        &readBuffer,
        sizeof(int),
        &bytesServiced,
        DummyCallback,
        &ioCount
    );
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << "Failed to read file " << result << endl;
    }
    else {
        cout << "Read posted" << endl;
    }

    while (ioCount != 2) {
        this_thread::yield();
    }

    cout << "Data has been read: " << readBuffer << endl;

    cout << "Benchmarking callback-based I/O" << endl;
    BenchmarkIO(
        store,
        fileId,
        maxFileSize
    );

    cout << "Benchmarking polling-based I/O" << endl;
    BenchmarkIOWithPolling(
        store,
        fileId,
        maxFileSize
    );

    return 0;
}