
/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kick_UsbFunctionFS.h"

#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <pthread.h>
    #include <errno.h>
    #include <stdint.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <string.h>
    #include <sys/wait.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <dirent.h>
    #include <limits.h>

#endif

namespace mt
{
#define MAX_FRAME_SIZE      64
#define MAX_FRAME_DATA_SIZE 60

    union FSdata
    {
        uint8_t raw[MAX_FRAME_SIZE];
        struct
        {
            uint32_t key;
            uint8_t  data[MAX_FRAME_SIZE - sizeof( uint32_t )];
        };
    };

    USBFunctionFS::USBFunctionFS()
        : juce::Thread( "USBFS" )
    {
        init();
    }

    USBFunctionFS::~USBFunctionFS()
    {
        signalThreadShouldExit();
        stopThread( 2000 );

        closeFileDescriptor( g_ep_write );
        closeFileDescriptor( g_ep_read );
    }

    void USBFunctionFS::init()
    {
        const juce::String& usbFilePath = "/dev/usb-utp0/ep0";
        juce::String        epSinkPath  = usbFilePath.substring( 0, usbFilePath.length() - 1 ) + "1";
        g_ep_write                      = openFileDescriptor( epSinkPath );
        if( g_ep_write < 0 )
            handleOpenFileError( epSinkPath );

        juce::String epSourcePath = usbFilePath.substring( 0, usbFilePath.length() - 1 ) + "2";
        g_ep_read                 = openFileDescriptor( epSourcePath );
        if( g_ep_read < 0 )
            handleOpenFileError( epSourcePath );
    }

    void USBFunctionFS::run()
    {
        constexpr int timeoutSeconds = 15; // Adjust the timeout value as needed
	isConnected.store(false);
        while( !threadShouldExit() )
        {
            if( g_ep_read > 0 && g_ep_write > 0 )
            {
                if( !timerRunning )
                {
                    DBG( "STARting Timer" );
                    startTimer( timeoutSeconds * 1000 ); // Convert seconds to milliseconds
                    timerRunning = true;
                }

                juce::String command;
                readCommandFromUSB( command );
                DBG( "After read Command From USB" );
                if( !command.isEmpty() )
                {
                    DBG( "Command Recieved." );
                    responseReceived = true;
                    isConnected.store( true );
                    handleCommand( command );
                    stopTimer();
                    timerRunning = false;
                }
            }
            else
            {
                init();
            }
            DBG( "making response Recieved False" );
            responseReceived = false; // Reset flag for the next iteration

            // Sleep for a short duration to avoid busy-waiting
            juce::Thread::sleep( 100 );
        }
    }

    void USBFunctionFS::timerCallback()
    {
        DBG( "Timer Callback Called" );
        if( !responseReceived )
        {
            DBG( "No response from PC app within timeout period" );
            isConnected.store( false );
            signalThreadShouldExit();
            usbmsg_t data{};
            data.command_status   = mt ::USB_STATUS::U_TIMEOUT;
            data.downloadfilesize = 0;
            data.percentage       = 0;
            notifyStatus(data);
            timerRunning = false;
            stopTimer();
        }
        // Handle timeout here
    }

    void USBFunctionFS::start()
    {
        if( g_ep_write > 0 && g_ep_read > 0 )
        {
            startThread();
        }
    }

    void USBFunctionFS::stop()
    {
        stopTimer();
        timerRunning.store(false);
        responseReceived.store(true);
	    DBG("USB FUnction STop Called");
        if( g_ep_write > 0 && g_ep_read > 0 )
        {
            signalThreadShouldExit();
        }
    }
    int USBFunctionFS::openFileDescriptor( const juce::String& filePath )
    {
        int fd = -1;
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        fd = open( filePath.toRawUTF8(), O_RDWR );
#endif
        return fd;
    }

    void USBFunctionFS::closeFileDescriptor( int fd )
    {
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        if( fd >= 0 )
            ::close( fd );
#endif
    }

    void USBFunctionFS::handleOpenFileError( const juce::String& filePath )
    {
        DBG( "Can't open file " << filePath );
    }

    int USBFunctionFS::writeData( void* p, size_t size )
    {
        int result = -1;
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        auto* bytes = static_cast<const unsigned char*>(p);
        size_t sent = 0;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000.0;
        while(sent < size && !threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < deadline)
        {
            const auto count = ::write(g_ep_write, bytes + sent, size - sent);
            if(count > 0) sent += (size_t)count;
            else if(count < 0 && errno != EINTR && errno != EAGAIN) return -1;
            else juce::Thread::sleep(2);
        }
        result = sent == size ? (int)sent : -1;
#endif
        return result;
    }

    int USBFunctionFS::writeFile(int fp, void* p, size_t size)
    {
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        size_t written = 0;
        const auto* bytes = static_cast<const unsigned char*>(p);
        while(written < size && !threadShouldExit())
        {
            const auto count = ::write(fp, bytes + written, size - written);
            if(count > 0) written += (size_t)count;
            else if(count < 0 && errno == EINTR) continue;
            else return -1;
        }
        return written == size ? (int)written : -1;
#else
        return -1;
#endif
    }

    void USBFunctionFS::readCommandFromUSB( juce::String& command )
    {
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        constexpr int bufferSize = 512;
        char          buffer[bufferSize];
        memset( buffer, 0, bufferSize );

        int flags = fcntl( g_ep_read, F_GETFL, 0 );
        fcntl( g_ep_read, F_SETFL, flags | O_NONBLOCK );

        ssize_t bytesRead = ::read( g_ep_read, buffer, bufferSize - 1 );
        if( bytesRead > 0 )
        {
            buffer[bytesRead] = '\0';
            command           = juce::String( buffer );
        }
        else
        {
            DBG( "Failed to read comamnd from Read FD" );
        }
#endif
    }

#if (defined( JUCE_LINUX ) || defined( __linux__ )) && !defined(KICK_DESKTOP)
    pid_t USBFunctionFS::popen2( const char* command, int* infp, int* outfp )
    {
        int   p_stdin[2], p_stdout[2];
        pid_t pid;

        if( pipe( p_stdin ) != 0 || pipe( p_stdout ) != 0 )
            return -1;

        pid = fork();

        if( pid < 0 )
            return pid;
        else if( pid == 0 )
        {
            close( p_stdin[WRITE] );
            if( infp == NULL )
                close( p_stdin[READ] );
            else
                dup2( p_stdin[READ], READ );
            close( p_stdout[READ] );
            if( outfp == NULL )
                close( p_stdout[WRITE] );
            else
                dup2( p_stdout[WRITE], WRITE );

            execl( "/bin/sh", "sh", "-c", command, NULL );
            perror( "execl" );
            exit( 1 );
        }

        if( infp == NULL )
            close( p_stdin[WRITE] );
        else
            *infp = p_stdin[WRITE];

        if( outfp == NULL )
            close( p_stdout[READ] );
        else
            *outfp = p_stdout[READ];

        close( p_stdin[READ] );
        close( p_stdout[WRITE] );
        return pid;
    }
#endif

    int USBFunctionFS ::sendAck()
    {
        union FSdata fs;
        fs.key = OKAY;
        // Send the result back
        writeData( &fs, 4 );
        // Implement your Ack handling logic here
        return 0;
    }

    int USBFunctionFS::handleWFileCommand(std::string& filename, size_t size)
    {
        FSdata response{};
        response.key = FAIL;
        bool opened = false;
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        // The host chooses a basename; the device owns the staging directory.
        const auto name = juce::String(filename).replaceCharacter('\\', '/').fromLastOccurrenceOf("/", false, false);
        const juce::File staging(PCMODE_PATH);
        if(size > 0 && size <= 2ull * 1024 * 1024 * 1024 && name.endsWithIgnoreCase(".img")
            && name == juce::File::createLegalFileName(name) && staging.createDirectory().wasOk())
        {
            const int directory = ::open(PCMODE_PATH, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            if(directory >= 0)
            {
                if(write_filefd >= 0) ::close(write_filefd);
                if(!pendingDownload.empty()) ::unlink(pendingDownload.c_str());
                const auto temporaryName = "download-" + juce::Uuid().toString() + ".part";
                write_filefd = ::openat(directory, temporaryName.toRawUTF8(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, S_IRUSR | S_IWUSR);
                opened = write_filefd >= 0;
                if(opened)
                {
                    pendingDownload = staging.getChildFile(temporaryName).getFullPathName().toStdString();
                    completedDownload = staging.getChildFile(name).getFullPathName().toStdString();
                }
                ::close(directory);
            }
        }
#endif
        downloadedSize = 0;
        tfilesize = opened ? size : 0;
        transferFailed = !opened;
        if(opened) response.key = OKAY;
        writeData(&response, 4);
        usbmsg_t status{};
        status.command_status = opened ? USB_STATUS::U_INIT : USB_STATUS::U_FAILED;
        status.totalfilesize = tfilesize;
        notifyStatus(status);
        return opened ? 0 : -1;
    }

    int USBFunctionFS::handleWDataCommand(uint32_t size)
    {
        FSdata response{};
        response.key = FAIL;
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        if(transferFailed || write_filefd < 0 || size == 0 || size > 8 * 1024 * 1024 || downloadedSize > tfilesize || size > tfilesize - downloadedSize)
        { writeData(&response, 4); return -1; }
        std::vector<unsigned char> bytes(size);
        response.key = DATA;
        snprintf((char*)response.data, sizeof(response.data), "%08X", size);
        if(writeData(&response, 12) != 12) return -1;
        size_t received = 0;
        auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000.0;
        while(received < size && !threadShouldExit() && juce::Time::getMillisecondCounterHiRes() < deadline)
        {
            const auto count = ::read(g_ep_read, bytes.data() + received, size - received);
            if(count > 0) { received += (size_t)count; deadline = juce::Time::getMillisecondCounterHiRes() + 15000.0; }
            else if(count < 0 && errno != EAGAIN && errno != EINTR) break;
            else juce::Thread::sleep(2);
        }
        const bool complete = received == size && writeFile(write_filefd, bytes.data(), received) == (int)received;
        if(complete) downloadedSize += received;
        transferFailed |= !complete;
        response.key = complete ? OKAY : FAIL;
        writeData(&response, 4);
        usbmsg_t status{};
        status.command_status = complete ? USB_STATUS::U_DOWNLOAD : USB_STATUS::U_FAILED;
        status.totalfilesize = tfilesize;
        status.downloadfilesize = downloadedSize;
        status.percentage = tfilesize ? 100.0 * double(downloadedSize) / double(tfilesize) : 0.0;
        notifyStatus(status);
        return complete ? 0 : -1;
#else
        writeData(&response, 4);
        return -1;
#endif
    }

    int USBFunctionFS::handleReadFileCommand( std::string path )
    {
        DBG( "In ReadFile Command" << path );
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        union FSdata  fs;
        std::ifstream file;
        size_t        size = 0;
        struct stat   st;
        int           rz = 4;
        file             = std::ifstream( path.c_str(), std::ios::binary );
        size             = calculateSizeofFile( &file );
        memset( &st, 0, sizeof( st ) );
        stat( path.c_str(), &st );
        size = st.st_size;
        sprintf( (char*)fs.data, "%016zX", size );
        rz = 4 + strlen( (char*)fs.data );
        printf( "filesize:%d\n", size );

        if( !file.is_open() )
        {
            fs.key = FAIL;
            DBG( "file doesn't exists" );
        }
        else
        {
            DBG( "file exists" );
            file.seekg( 0, std::ios::beg );
            file.close();
            fs.key = OKAY;
            writeData( &fs, rz );
        }
#endif
        return 0;
    }

    int USBFunctionFS::handleCloseFileCommand()
    {
        bool complete = false;
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)
        if(write_filefd >= 0)
        {
            complete = !transferFailed && tfilesize > 0 && downloadedSize == tfilesize && ::fsync(write_filefd) == 0;
            complete = (::close(write_filefd) == 0) && complete;
            write_filefd = -1;
            if(complete) complete = ::rename(pendingDownload.c_str(), completedDownload.c_str()) == 0;
        }
        if(!complete && !pendingDownload.empty()) ::unlink(pendingDownload.c_str());
#endif
        pendingDownload.clear();
        FSdata response{};
        response.key = complete ? OKAY : FAIL;
        writeData(&response, 4);
        usbmsg_t status{};
        status.command_status = complete ? USB_STATUS::U_DONE : USB_STATUS::U_FAILED;
        status.totalfilesize = tfilesize;
        status.downloadfilesize = downloadedSize;
        status.percentage = complete ? 100.0 : 0.0;
        notifyStatus(status);
        return complete ? 0 : -1;
    }

    int USBFunctionFS::handlPCstartOpration()
    {
        sendAck();
        return 0;
    }

    int USBFunctionFS::handlPCendOpration()
    {
        sendAck();
        return 0;
    }

    int USBFunctionFS::handleCommand( const juce::String& command )
    {
        union FSdata fs;
        usbmsg_t     data{};
        // Implement your command handling logic here
        uint32_t status;
        int      flags;
#if (defined( JUCE_LINUX ) || defined( __linux__ )) && !defined(KICK_DESKTOP)
        fd_set         rfds;
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 50000;
#endif
        std::ifstream file;
        const char*   buff = command.toRawUTF8();
        DBG( "Command Recieved " << command );
        if( strncmp( buff, "Ack:", 4 ) == 0 )
        {
            DBG( "ACk Recived from app" );
            const char* ptr = buff + 4; // Move the pointer to skip "WFile:"

            // Extract the filename
            std::string KickVersion( ptr );

            DBG( "KickVersion" << KickVersion );
	    
            mt::FirmwareUpdate::getSwupdateInstance().setPcSwVersion( KickVersion );
            sendAck();
        }
        else if( strncmp( buff, "PC_OPERATION_START", 18 ) == 0 )
        {
            handlPCstartOpration();
            DBG( "PC app started comm" );
        }
        else if( strncmp( buff, "PC_OPERATION_END", 16 ) == 0 )
        {
            handlPCendOpration();
            DBG( "PC app stoped comm" );
        }
        else if( strncmp( buff, "WFile:", 6 ) == 0 )
        {
            DBG( "IN Wfile Command" );
            const char* ptr = buff + 6; // Move the pointer to skip "WFile:"

            // Find the position of ':' after filename
            const char* colonPtr = strchr( ptr, ':' );
            if( colonPtr == nullptr )
            {
                DBG( "Invalid command format: Missing ':'" );
                // Handle the error or return
                return -1;
            }

            // Extract the filename
            std::string filename( ptr, colonPtr - ptr );

            // Extract the size
            std::string sizeStr( colonPtr + 1 ); // Points to the character after ':'
            try
            {
                size_t parsed = 0;
                const auto size = std::stoull(sizeStr, &parsed);
                if(parsed != sizeStr.size() || size > 2ull * 1024 * 1024 * 1024) return -1;
                handleWFileCommand(filename, (size_t)size);
            }
            catch(const std::exception&) { return -1; }
        }
        else if( strncmp( buff, "Wdata:", 6 ) == 0 ) //download
        {
            DBG( "In Wdata Command " );
            uint32_t data_size; /* download:0x100000 */
            char* end = nullptr;
            errno = 0;
            const auto parsed = strtoull(buff + 6, &end, 16);
            if(errno || end == buff + 6 || *end != '\0' || parsed > 8 * 1024 * 1024) return -1;
            data_size = (uint32_t)parsed;
            handleWDataCommand( data_size );
        }
        else if( strncmp( buff, "FirmwareComplete", 16 ) == 0 )
        {
            handleCloseFileCommand();
        }
        else if( strncmp( buff, "Closefile", 9 ) == 0 ) //close
        {
            handleCloseFileCommand();
            DBG( "In Closefile Command " );
        }
        else if( strncmp( buff, "ReadFile:", 9 ) == 0 )
        {
            DBG( "ReadFile " << buff );
#if (( JUCE_LINUX || __linux__ )) && !defined(KICK_DESKTOP)

            const char* filePath = buff + 9;
            std::string path     = filePath;
            handleReadFileCommand( filePath );

#endif
        }
        else if( strncmp( buff, "CREATE_DIR", 10 ) == 0 )
        {
            DBG( "Create DIR comamnd Recieved" );
        }

        else if( strncmp( buff, "UCmd:", 5 ) == 0 )
        {
            DBG( "Ucmd" );
        }
        else if( strncmp( buff, "upload", 6 ) == 0 )
        {
            DBG( "Not Applicable for Kick" );
        }
        else if( strncmp( buff, "DeviceInfo:", 11 ) == 0 )
        {
            handleDeviceInfoCommand();
        }
        else if( strncmp( buff, "FactoryReset", 12 ) == 0 )
        {
            DBG( "Not Applicable for Kick" );
        }
        else
        {
            printf( "Unknow Cmd Recieved from PC %s\n", buff );
        }

        return 0;
    }

    size_t USBFunctionFS::calculateSizeofFile( std::ifstream* file )
    {
        size_t size;
        file->seekg( 0, std::ios::end );
        size = file->tellg();
        file->seekg( 0, std::ios::beg );
        return size;
    }

    int USBFunctionFS::handleDeviceInfoCommand()
    {
        // pc requesting device status info like ram status,internal and usb status
        union FSdata   fs;
        mt::DeviceInfo info = mt::HwPref::getHwPref().getDeviceInfo();

        writeData( info.serialNumber, sizeof( info.serialNumber ) ); // Send the DeviceInfo struct data
        printf( "Serial Number data: %s\n", info.serialNumber );

        writeData( info.firmwareVersion, sizeof( info.firmwareVersion ) );
        printf( "firmwareVersion data: %s\n", info.firmwareVersion );

        writeData( info.firmwareBuildDate, sizeof( info.firmwareBuildDate ) );
        printf( "firmwareBuildDate data: %s\n", info.firmwareBuildDate );

        writeData( info.memoryStatus.total, sizeof( info.memoryStatus.total ) ); // Send memory status struct
        printf( "memoryStatus.total data: %s\n", info.memoryStatus.total );

        writeData( info.memoryStatus.available, sizeof( info.memoryStatus.available ) ); // Send memory status struct
        printf( "memoryStatus.available data: %s\n", info.memoryStatus.available );

        writeData( info.memoryStatus.used, sizeof( info.memoryStatus.used ) ); // Send memory status struct
        printf( "memoryStatus.used data: %s\n", info.memoryStatus.used );

        writeData( info.internalStorageStatus.totalSpace, sizeof( info.internalStorageStatus.totalSpace ) ); // Send internal storage status struct
        printf( "internalStorageStatus.totalSpace data: %s\n", info.internalStorageStatus.totalSpace );

        writeData( info.internalStorageStatus.availableSpace, sizeof( info.internalStorageStatus.availableSpace ) ); // Send internal storage status struct
        printf( "internalStorageStatus.availableSpace data: %s\n", info.internalStorageStatus.availableSpace );

        writeData( info.internalStorageStatus.usedSpace, sizeof( info.internalStorageStatus.usedSpace ) ); // Send internal storage status struct
        printf( "internalStorageStatus.usedSpace data: %s\n", info.internalStorageStatus.usedSpace );

        writeData( info.usbStorageStatus.totalSpace, sizeof( info.usbStorageStatus.totalSpace ) );
        printf( "usbStorageStatus.totalSpace data: %s\n", info.usbStorageStatus.totalSpace );

        writeData( info.usbStorageStatus.availableSpace, sizeof( info.usbStorageStatus.availableSpace ) );
        printf( "usbStorageStatus.availableSpace data: %s\n", info.usbStorageStatus.availableSpace );

        fs.key = OKAY;
        writeData( &fs, 4 );

        return 0;
    }

#undef MAX_FRAME_SIZE
#undef MAX_FRAME_DATA_SIZE

}
