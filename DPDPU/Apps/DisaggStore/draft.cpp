//HANDLE hFile;
        //// create the file.
        //// noet: we have to create a new file otherwise it will return code 183 which means file already exists
        //hFile = CreateFile(TEXT("test.TXT"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
        //if (hFile != INVALID_HANDLE_VALUE)
        //{
        //    DWORD dwByteCount;
        //    TCHAR szBuf[64] = TEXT("/0");
        //    // Write a simple string to hfile.
        //    for (int i = 0; i < 10000; i++) {
        //        WriteFile(hFile, "This is a test message", 25, &dwByteCount, NULL);
        //    }
        //    // Set the file pointer back to the beginning of the file.
        //    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
        //    //// Read the string back from the file.
        //    //ReadFile(hFile, szBuf, 128, &dwByteCount, NULL);
        //    //// Null terminate the string.
        //    //szBuf[dwByteCount] = 0;
        //    //// Close the file.
        //    ////output message with string if successful
        //    cout << "created test.txt" << endl;
        //    //SetFilePointer(hFile, 0, 0, FILE_BEGIN);
        //    ////memset(szBuf, 0, 64);
        //    //DWORD byte;
        //    ////TCHAR Buf[64] = TEXT("/0");
        //    //for (int i = 1; i < 3; i++) {
        //    //    ReadFile(hFile, szBuf, 8, &byte, NULL);
        //    //    szBuf[byte] = 0;
        //    //    printf("%s\n", szBuf);
        //    //    //_tprintf(TEXT("%s\n", s);Buf);
        //    //    //std::wcout << "reading" << szBuf << endl;
        //    //    //SetFilePointer(hFile, 2, 0, FILE_CURRENT);
        //    //}
        //    CloseHandle(hFile);
        //}
        //else
        //{
        //    //output message if unsuccessful
        //    cout << "creation failed" << endl;
        //}

        // const int Size = 100;
        // GenerateFile(Size);

        //for (int i = 0; i < 10000; i++) {
        //    /*HANDLE *hFile = (HANDLE*)malloc(sizeof(HANDLE));*/
        //    HANDLE hFile = CreateFile(TEXT("random.txt"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        //    if (hFile == INVALID_HANDLE_VALUE) {
        //        std::cerr << "Error opening the file. Error code: " << GetLastError() << std::endl;
        //        return 1;
        //    }
        //    
        //    char *buffer = (char*)malloc(1024*Size+1);
        //    DWORD bytesRead;
        //    SetFilePointer(hFile, Size *1024*0, 0, FILE_BEGIN);

        //    OVERLAPPED* overlapped = new OVERLAPPED;
        //    ZeroMemory(overlapped, sizeof(OVERLAPPED));

        //    FileReadData *readData = new FileReadData;
        //    readData->hFile = hFile;
        //    readData->buffer = buffer;
        //    overlapped->hEvent = reinterpret_cast<HANDLE>(&readData);
        //    cout << "start to read file" << endl;
        //    if (ReadFileEx(hFile, buffer, sizeof(buffer), overlapped, FileReadCompletion)) {
        //        // ReadFileEx completed synchronously.
        //        // You can handle it here if needed.
        //        cout << "sync branch in " << i << endl;
        //        /*cout.write(buffer, 1000);
        //        cout << endl;*/
        //        CloseHandle(hFile);
        //        free(buffer);
        //        delete overlapped;
        //        delete readData;
        //    }
        //    //else {
        //    //    cout << "async branch in " << i << endl;
        //    //    if (GetLastError() != ERROR_IO_PENDING) {
        //    //        std::cerr << "Error starting asynchronous read. Error code: " << GetLastError() << std::endl;
        //    //    }
        //    //    else {
        //    //        // Asynchronous read has been initiated. Wait for completion.
        //    //        WaitForSingleObject(overlapped->hEvent, INFINITE);
        //    //        cout << "finished" << endl;
        //    //    }
        //    //}
        //}
        /*if (ReadFileEx(hFile, buffer, sizeof(buffer), &overlapped, FileReadCompletion)) {
            std::cerr << "Error starting asynchronous read. Error code: " << GetLastError() << endl;
        }*/

        // Continue with other tasks without waiting for the read to complete

        // Optionally, you can wait for the read operation to complete using a sleep or other synchronization mechanism.

        // Close the file handle when done

        // CloseHandle(hFile);