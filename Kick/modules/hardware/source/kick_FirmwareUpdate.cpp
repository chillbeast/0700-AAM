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
#include "kick_FirmwareUpdate.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace mt
{

    FirmwareUpdate::FirmwareUpdate()
    {
        hwMessage.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM;
        hwMessage.pktCmd  = contracts::DeviceControlContracts::SysCmd::EN_SYS_SW_UPDATE;
        hwMessage.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    }

    int FirmwareUpdate::connect_ipc( bool reconnect )
    {
        int fd = -1;
#if (defined( __linux__ ) || defined( __linux ) || defined( linux )) && !defined(KICK_DESKTOP)
        DBG( "IN CONNECT_IPC " );
        struct sockaddr_un servaddr;
        fd = socket( AF_LOCAL, SOCK_STREAM, 0 );
        bzero( &servaddr, sizeof( servaddr ) );
        servaddr.sun_family = AF_LOCAL;
        strncpy( servaddr.sun_path, socketpath, sizeof( servaddr.sun_path ) - 1 );

        /*
         * Check to get a valid socket
         */
        if( fd < 0 )
            return -1;

        do
        {
            if( connect( fd, (struct sockaddr*)&servaddr, sizeof( servaddr ) ) == 0 )
            {
                break;
            }
            if( !reconnect )
            {
                DBG( "cannot communicate with SWUpdate via " << socketpath );
                exit( 1 );
            }
            usleep( 1000 );
        } while( true );

        DBG( "Connected to SWUpdate via=" << socketpath );
#endif
        return fd;
    }

    int FirmwareUpdate::progress_ipc_receive( int* connfd, struct progress_msg* msg )
    {
        int ret = -1;
#if (defined( __linux__ ) || defined( __linux ) || defined( linux )) && !defined(KICK_DESKTOP)
        ret = read( *connfd, msg, sizeof( *msg ) );
        DBG( "ret-> " << ret );
        if( ret != sizeof( *msg ) )
        {
            DBG( "Connection closing..\n" );
            close( *connfd );
            *connfd = -1;
            return -1;
        }
#endif
        return ret;
    }

    MCUUpdateAction FirmwareUpdate::getMcuUpdateState( int f_data )
    {
        MCUUpdateAction returnData = MCUUpdateAction::EN_SW_UPDATE_END;
        if( f_data == 0 )
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_END;
        }
        else if( f_data > 0 )
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_DATA;
        }
        else if( f_data == -2 )
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_DATABASE;
        }
        else
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_END;
        }
        return ( returnData );
    }

    void FirmwareUpdate::updateFirmware()
    {
        if( updateInProgress.exchange(true) ) return;
        struct Reset { std::atomic<bool>& flag; ~Reset() { flag.store(false); } } reset{updateInProgress};
        cancelRequested.store(false);
        bool mcuFwStatus = ( isMCUImagePresent() && mt::HwPref::getHwPref().getSurfaceVersion().isNotEmpty() );

#if (defined( __linux__ ) || defined( __linux ) || defined( linux )) && !defined(KICK_DESKTOP)
        if( juce::File( "/tmp/fwupdate/Kick.swu" ).exists() )
        {
            DBG( "In Firmware Upgrade" );

            int status = linuxUpdate();
            if( status != SUCCESS )
            {
                DBG( " Linux FwUpdate SuccessFul" );
                return;
            }
            if( mcuFwStatus )
            {
                DBG( "IN MCU UPDATE NOW" );

                int mcuUpdate = MCUUpdate( mcuFilePath );
                if( mcuUpdate == 1 )
                {
                    DBG( "MCUUPDATE DONE" );
                }
                else if( mcuUpdate == -1 )
                {
                    DBG( "MCUUPDATE FAILED" );
                }
            }
        }
#endif
        DBG( "FirmWare upgrade Successful" );
    }

    int FirmwareUpdate::MCUUpdate( juce::String& McuFilePath )
    {
        int               SwUpdateSuccess = -1;
        mt::swupdatemsg_t data{};

        DBG( "MCU UPDATE" );
        bool               boSwUpdateStartUpCheck = false;
        char               binaryData[32]         = { 0 };
        int                totalRead              = 0;
        juce::int64        currentPos             = 0;
        int                hexRecSize             = 43;
        juce::int64        fileSize               = 0;

        boDataTransferPending = true;
        boUpdateComplete = false;
        // Validate the whole file before sending an initiate command.
        parser.AddPath(McuFilePath);
        int validation = 0;
        do { validation = parser.readNext(binaryData, sizeof(binaryData)); } while( validation > 0 || validation == -2 );
        if(validation != 0)
        {
            data.command_status = RECOVERY_STATUS::FAILURE;
            notifyStatus(data);
            return -1;
        }
        auto lastActivity = juce::Time::getMillisecondCounterHiRes();
        //if(CheckForVersion() == 1)
        {
            if( !boSwUpdateStartUpCheck )
            {
                boSwUpdateStartUpCheck = true;
                //DBG( "First Time" );
                parser.AddPath( McuFilePath );
                fileSize = juce::jmax<juce::int64>(1, parser.getSize() / hexRecSize);
                //DBG("FileSize" << fileSize);
                if( fileSize == 0 )
                    jassertfalse;

                currentPos += 1;
                totalProgress    = (int)( 100 * currentPos / fileSize );
                boUpdateComplete = false;
                mcuUpdateState   = MCUUpdateAction::EN_SW_UPDATE_INITIATE;
            }

            while( boDataTransferPending )
            {
                //DBG("Sw Update State Machine");
                if( cancelRequested.load() || juce::Time::getMillisecondCounterHiRes() - lastActivity > 10000.0 )
                    mcuUpdateState.store(MCUUpdateAction::EN_SW_UPDATE_FAILURE);
                if( mcuUpdateState.load() != MCUUpdateAction::EN_SW_UPDATE_IDLE )
                    lastActivity = juce::Time::getMillisecondCounterHiRes();
                switch( mcuUpdateState.load() )
                {
                    ////////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_INITIATE:
                        ////////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Initiate" );
#endif
                            auto hwMsg =
                                contracts::DclContractsHelper::SendSwUpdateCmdMessage( (int)MCUUpdateAction::EN_SW_UPDATE_INITIATE );

                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            contracts::DclSignals::sendCommandToDcl( hwMsg );
                            break;
                        }
                    /////////////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_BEGIN_TRANSFER:
                        /////////////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Begin Transfer" );
#endif
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_ACK;
                        }
                        break;
                    ///////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_DATA:
                        ///////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Data" );
#endif
                            std::vector<int> sendData;
                            sendData.push_back( MCUUpdateAction::EN_SW_UPDATE_DATA );
                            for( unsigned int i = 0; i < (unsigned int)totalRead; i++ )
                            {
                                sendData.push_back( static_cast<unsigned char>(binaryData[i]) );
                            }
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            hwMessage.data = sendData;

                            contracts::DclSignals::createAndSendCommandToDcl( hwMessage );

                            currentPos += 1;
                            // Update the progres bar
                            data.command_status = mt::RECOVERY_STATUS::RUN;
                            data.nsteps         = linuxsteps; //last step
                            data.curstep        = linuxsteps;
                            double _progress    = ( (float)currentPos / (float)fileSize ) * 100;

                            // Calculate the remaining percentage
                            double remainingPercentage = 100.0 - linuxPercentage;

                            // Calculate the remaining percentage based on _progress
                            double remainingFromProgress = remainingPercentage * ( _progress / 100.0 );

                            // Update total progress by adding the remaining percentage from _progress
                            totalProgress = linuxPercentage + remainingFromProgress;

                            data.totalper = totalProgress;

#ifdef DEBUG_FWUPDATE
                            DBG( "Linux Stpes -> " << linuxsteps );
                            DBG( "remaining Percentage" << remainingPercentage );
                            DBG( "remaining From Progress" << remainingFromProgress );
                            DBG( "Total Progress with MCU " << totalProgress );
#endif
                            notifyStatus(data);

                            break;
                        }
                    ////////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_DATABASE:
                        ///////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "DataBase" );
#endif
                            std::vector<int> sendBaseData;
                            sendBaseData.push_back( MCUUpdateAction::EN_SW_UPDATE_DATABASE );
                            for( unsigned int i = 0; i < 2; i++ )
                            {
                                sendBaseData.push_back( (int)binaryData[i] );
                            }
                            hwMessage.data = sendBaseData;
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            contracts::DclSignals::createAndSendCommandToDcl( hwMessage );
                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_END:
                        ///////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "SUCCESS -> END" );
#endif
                            std::vector<int> sendBaseData;
                            sendBaseData.push_back( MCUUpdateAction::EN_SW_UPDATE_END );
                            hwMessage.data = sendBaseData;
                            contracts::DclSignals::createAndSendCommandToDcl( hwMessage );
                            boUpdateComplete       = true;
                            boSwUpdateStartUpCheck = false;
                            boDataTransferPending  = false;
                            mcuUpdateState         = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            SwUpdateSuccess        = 1;
                            data.command_status    = mt::RECOVERY_STATUS::SUCCESS;
                            data.totalper          = 100.0;
                            notifyStatus(data);

                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_ACK:
                        ///////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Ack" );
#endif
                            totalRead      = parser.readNext( binaryData );
                            mcuUpdateState = totalRead == -3 ? MCUUpdateAction::EN_SW_UPDATE_FAILURE : getMcuUpdateState( totalRead );

                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_IDLE:
                        ///////////////////////////////////
                        {
                            mcuStateChanged.wait(20);
                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_FAILURE:
                        ///////////////////////////////////
                        {
                            SwUpdateSuccess        = -1;
                            boSwUpdateStartUpCheck = false;
                            boDataTransferPending  = false;
                            data.command_status    = mt::RECOVERY_STATUS::FAILURE;
                            notifyStatus(data);
                            DBG( "Failure" );
                            break;
                        }
                }
            }
        }
        return SwUpdateSuccess;
    }

    void FirmwareUpdate::HexParser::AddPath( const juce::String& filePath )
    {
        fileStream.reset( new juce::FileInputStream( juce::File( filePath ) ) );
        if( !fileStream->openedOk() )
        {
#ifdef PREF_DBG
            DBG( "failed to open firmware file" );
#endif()
        }
    }

    bool FirmwareUpdate::isMCUImagePresent()
    {
        if( juce::File( mcuFilePath ).exists() )
        {
            return true;
        }
        return false;
    }

    int FirmwareUpdate::HexParser::readNext( char redData[], size_t capacity )
    {
        if( !fileStream || !fileStream->openedOk() ) return -3;
        for(;;)
        {
            juce::String line;
            while( !fileStream->isExhausted() )
            {
                const auto c = fileStream->readByte();
                if( c == '\n' ) break;
                if( c != '\r' ) line += juce::String::charToString(c);
                if( line.length() > 521 ) return -3;
            }
            if( line.isEmpty() )
            {
                if( fileStream->isExhausted() ) return -3; // EOF record is required.
                continue;
            }
            if( !line.startsWithChar(':') || line.length() < 11 || (line.length() & 1) == 0 ) return -3;
            std::array<unsigned, 260> bytes{};
            unsigned sum = 0;
            const int count = (line.length() - 1) / 2;
            auto hex = [](juce::juce_wchar c) -> int {
                if( c >= '0' && c <= '9' ) return int(c - '0');
                if( c >= 'A' && c <= 'F' ) return int(c - 'A' + 10);
                if( c >= 'a' && c <= 'f' ) return int(c - 'a' + 10);
                return -1;
            };
            for( int i = 0; i < count; ++i )
            {
                const int high = hex(line[1 + i * 2]), low = hex(line[2 + i * 2]);
                if( high < 0 || low < 0 ) return -3;
                sum += bytes[(size_t)i] = unsigned(high * 16 + low);
            }
            const auto size = bytes[0], type = bytes[3];
            if( count != int(size + 5) || (sum & 255) != 0 ) return -3;
            if( type == 1 ) return size == 0 && bytes[1] == 0 && bytes[2] == 0 ? 0 : -3;
            if( type == 4 )
            {
                if( size != 2 || capacity < 2 || bytes[1] || bytes[2] ) return -3;
                redData[0] = char(bytes[4]); redData[1] = char(bytes[5]);
                return -2;
            }
            if( type == 3 || type == 5 ) { if( size != 4 ) return -3; continue; }
            if( type != 0 || size + 2 > capacity ) return -3;
            redData[0] = char(bytes[1]); redData[1] = char(bytes[2]);
            for( unsigned i = 0; i < size; ++i ) redData[i + 2] = char(bytes[i + 4]);
            return int(size + 2);
        }
    }

    juce::int64 FirmwareUpdate::HexParser::getSize()
    {
        if( !fileStream || !fileStream->getStatus().wasOk() )
            return 0;

        return fileStream->getTotalLength();
    }

    int FirmwareUpdate::linuxUpdate( bool status )
    {
#ifdef KICK_DESKTOP
        juce::ignoreUnused(status);
        return FAILURE;
#endif
        int fwstatus = SUCCESS;
        DBG( "IN FIRMWRE UPDATE" );
        swupdatemsg_t data{};
        data.imageType               = SwupdateType_t::LINUXUPDATE;
        static double _totalProgress = 0.0;
        bool          mcustaus       = ( isMCUImagePresent() && mt::HwPref::getHwPref().getSurfaceVersion().isNotEmpty() );
        ;
#if (defined( __linux__ ) || defined( __linux ) || defined( linux )) && !defined(KICK_DESKTOP)
        DBG( "Firmware update confirmed by user... Startting upgrade script..." );
        int                 connfd = -1;
        struct progress_msg msg;

        std::string filePath = "/tmp/fwupdate/Kick.swu -v &";
        std::string command  = "/usr/bin/swupdate" + std::string( " -i " ) + filePath;

        std::future<void> future = std::async( std::launch::async, [&command] {
            // Execute the command
            std::system( command.c_str() );
        } );

        sleep( 1 );
        while( 1 )
        {
            if( juce::File( socketpath ).exists() )
            {
                DBG( "FIle Present" );
                break;
            }
            else
            {
                DBG( "File Not present" );
            }
            sleep( 100 );
        }

        while( 1 )
        {
            if( connfd < 0 )
            {
                connfd = connect_ipc( true );
            }
            /*
              * if still fails, try later
              */
            if( connfd < 0 )
            {
                sleep( 1 );
                continue;
            }

            if( progress_ipc_receive( &connfd, &msg ) == -1 )
            {
                continue;
            }

            /*
             * Be sure that string in message are Null terminated
             */
            if( msg.infolen > 0 )
            {
                if( msg.infolen >= sizeof( msg.info ) - 1 )
                {
                    msg.infolen                      = sizeof( msg.info ) - 1;
                    msg.info[sizeof( msg.info ) - 1] = '\0';
                }
                fprintf( stdout, "INFO : %s\n\n", msg.info );
            }
            msg.cur_image[sizeof( msg.cur_image ) - 1] = '\0';

            data.command_status = msg.status;
            std::strncpy( data.curimage, msg.cur_image, sizeof( data.curimage ) - 1 );
            data.curimage[sizeof( data.curimage ) - 1] = '\0'; // Ensure null-termination

            data.cur_percent = msg.cur_percent;
            if( mcustaus )
            {
                data.nsteps = ( msg.nsteps + 1 );
            }
            else
            {
                data.nsteps = msg.nsteps;
            }
            data.curstep = msg.cur_step;
    #ifdef DEBUG_FWUPDATE
            DBG( "========================END========================" );
            DBG( "msg.cur_percent-> " << juce::String( msg.cur_percent ) );
            DBG( "msg.cur_image-> " << juce::String( msg.cur_image ) );
            DBG( "msg.cur_step-> " << juce::String( msg.cur_step ) );
            DBG( "msg.nsteps-> " << juce::String( msg.nsteps ) );

            DBG( "========================END========================" );
    #endif
            if( msg.status != mt::RECOVERY_STATUS::IDLE )
            {
                // Calculate overall progress
                _totalProgress = ( static_cast<double>( msg.cur_step - 1 ) / data.nsteps ) * 100.0;
                _totalProgress = _totalProgress + ( ( ( static_cast<double>( msg.cur_percent ) / 100.0 ) ) * static_cast<double>( 100.0 / data.nsteps ) );
                data.totalper  = _totalProgress;

                // DBG( "Progress ->  " << juce::String( data.totalper ) );
                if( true )
                {
                    if( msg.status == mt::RECOVERY_STATUS::SUCCESS )
                    {
                        /* if mcu image is not present we need to send file status as failure*/
                        if( mcustaus ) { data.command_status = mt::RECOVERY_STATUS::LINUX_SUCCESS; }
                        else { data.command_status = mt::RECOVERY_STATUS::SUCCESS; }

                        linuxsteps      = data.nsteps;
                        linuxPercentage = _totalProgress;
                        notifyStatus(data);
                        break;
                    }
                    else if( msg.status == mt::RECOVERY_STATUS::FAILURE )
                    {
                        /* if mcu image is not present we need to send file status as failure*/
                        if( mcustaus ) { data.command_status = mt::RECOVERY_STATUS::FAILURE; }
                        else { data.command_status = mt::RECOVERY_STATUS::LINUX_FAILURE; }

                        notifyStatus(data);
                        fwstatus = FAILURE;
                        break;
                    }
                    notifyStatus(data);
                }
            }
        }
#endif
        return fwstatus;
    }

    SwUpdateErrorCode_t FirmwareUpdate::validateImage( bool downloaded )
    {
        SwUpdateMode_t mode   = getSwUpdateMode();
        std::string    fwpath = getFwFilepath( mode );

        if( mode == SwUpdateMode_t::PC )
        {
            if( downloaded )
            {
                std::vector<std::pair<std::string, mt::linuxFwInfo>> usbFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( PCMODE_PATH );
                if( !usbFiles.empty() )
                    fwpath = usbFiles[0].first;
            }
            else
            {
                DBG( "Firmware is not downloaded yet" );
                return SwUpdateErrorCode_t::SW_SUCCESS;
            }
        }

        if( mt::HwPref::getHwPref().decryptopenSSLImage( fwpath ) )
        {
            if( mt::HwPref::getHwPref().extractImage() )
            {
                DBG( "Extarction of Image Success" );
                return SwUpdateErrorCode_t::SW_SUCCESS;
            }
            else
            {
                DBG( "Extarction of Image Failed" );
                return SwUpdateErrorCode_t::IMAGE_EXTRACTION_E;
            }
        }
        else
        {
            DBG( "Image Validation Failed ERROR Code" << juce::String::formatted( "0x%04X", mt::SwUpdateErrorCode_t::IMAGE_DYCRYPTION_E ) );
            return SwUpdateErrorCode_t::IMAGE_DYCRYPTION_E;
        }

        DBG( "IMage Not found in " << fwpath );
        return SwUpdateErrorCode_t::IMAGE_NOTFOUND;
    }

    SwUpdateMode_t FirmwareUpdate::getSwUpdateModeFromString( const juce::String& deviceName )
    {
        if( deviceName == "PC" )
            return SwUpdateMode_t::PC;
        else if( deviceName == "SDCard" )
            return SwUpdateMode_t::SDCARD;
        else if( deviceName == "USBDisk" )
            return SwUpdateMode_t::USBDISK;
        else if( deviceName == "OTA" )
            return SwUpdateMode_t::OTA;
        else
            return SwUpdateMode_t::NONE;
    }

    SwUpdateErrorCode_t FirmwareUpdate::setSwUpdateMode( SwUpdateMode_t id )
    {
        switch( id )
        {
            case SwUpdateMode_t::SDCARD:
                DBG( "SDCARD MODE IS SELECTED" );
                break;
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                break;
            case SwUpdateMode_t::PC:
                DBG( "PC MODE IS SELECTED" );
                break;
            default:
                DBG( "Invalid SwUpdate Mode" );
                return mt::SwUpdateErrorCode_t::SW_FAILED;
        }
        swUpdateMode = id;
        return mt::SwUpdateErrorCode_t::SW_SUCCESS;
    }

    std::string FirmwareUpdate::getFwFilepath( SwUpdateMode_t id )
    {
        switch( id )
        {
            case SwUpdateMode_t::SDCARD:
                DBG( "SDCARD MODE IS SELECTED" );
                if( !sd_card_firmware_files.empty() )
                    return sd_card_firmware_files[0].first; // Assuming the first file in the vector is the desired one
                else
                    return "";
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                if( !usb_firmware_files.empty() )
                    return usb_firmware_files[0].first; // Assuming the first file in the vector is the desired one
                else
                    return "";
            case SwUpdateMode_t::PC:
                return PCMODE_PATH;
            default:
                DBG( "Invalid SwUpdate Mode" );
                return "";
        }
    }

    std::string FirmwareUpdate::getFwFileVersion( SwUpdateMode_t id )
    {
        switch( id )
        {
            case SwUpdateMode_t::SDCARD:
                DBG( "SDCARD MODE IS SELECTED" );
                if( !sd_card_firmware_files.empty() )
                {
                    return "V" + std::to_string( sd_card_firmware_files.at( 0 ).second.major ) + "." + std::to_string( sd_card_firmware_files.at( 0 ).second.minor ) + "." + std::to_string( sd_card_firmware_files.at( 0 ).second.patch );
                }
                else
                {
                    return "";
                }
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                if( !usb_firmware_files.empty() )
                {
                    return "V" + std::to_string( usb_firmware_files.at( 0 ).second.major ) + "." + std::to_string( usb_firmware_files.at( 0 ).second.minor ) + "." + std::to_string( usb_firmware_files.at( 0 ).second.patch );
                }
                else
                {
                    return "";
                }
            case SwUpdateMode_t::PC:
                DBG( "PC MODE IS SELECTED" );
                return PCMODE_PATH; // Assuming PCMODE_PATH is a string constant representing the PC mode path
            default:
                DBG( "Invalid SwUpdate Mode" );
                return "";
        }
    }
    VersionComparisonResult FirmwareUpdate::compareVersion( std::string filePath )
    {
        std::string path = "/etc/";
        linuxFwInfo linuxinfo_f, linuxinfo;

        if( !HwPref::getHwPref().getLinuxVersion( path, linuxinfo ) )
        {
            DBG( "failed to get current fwversion" );
            return VersionComparisonResult::Error;
        }
        if( HwPref::getHwPref().getVersionfromFile( filePath, linuxinfo_f ) )
        {
            DBG( "Got the version from file" );
        }
        else
        {
            return VersionComparisonResult::Error;
        }
        return HwPref::getHwPref().isLinuxFirmwareVersionLess( linuxinfo, linuxinfo_f );
    }

    VersionComparisonResult FirmwareUpdate::compareVersion( std::string filePath, mt::linuxFwInfo finfo )
    {
        std::string path = "/etc/";
        linuxFwInfo linuxinfo_f, linuxinfo;

        if( !HwPref::getHwPref().getLinuxVersion( path, linuxinfo ) )
        {
            DBG( "failed to get current fwversion" );
            return VersionComparisonResult::Error;
        }
        return HwPref::getHwPref().isLinuxFirmwareVersionLess( linuxinfo, finfo );
    }
    std::string FirmwareUpdate::getPcSwVersion()
    {
        return pcFwVersion;
    }

    void FirmwareUpdate::setPcSwVersion( std::string version )
    {
        pcFwVersion = version;
    }

    void FirmwareUpdate::startFirmwareScan()
    {
        DBG( "Scannning firmware" );
        pcFwVersion.clear();
        usb_firmware_files.clear();
        sd_card_firmware_files.clear();
        pcAppStatus = false;
        // Start firmware scan on USB drives
        usb_firmware_files = scanFirmwareOnUSBDrives();

        // Start firmware scan on SD cards
        sd_card_firmware_files = scanFirmwareOnSDCards();
        // Start firmware scan in PC app
        pcAppStatus = scanFirmwareOnPCApp();
    }

    bool FirmwareUpdate::isFirmwarePresent( SwUpdateMode_t mode )
    {
        switch( mode )
        {
            case SwUpdateMode_t::SDCARD:
                if( !sd_card_firmware_files.empty() )
                    return true; // Assuming the first file in the vector is the desired one
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                if( !usb_firmware_files.empty() )
                    return true; // Assuming the first file in the vector is the desired one
            case SwUpdateMode_t::PC:
                DBG( "PC MODE IS SELECTED" );
                if( !pcFwFile.empty() )
                    return true;
            default:
                DBG( "Invalid SwUpdate Mode" );
                return false;
        }
        return false;
    }

    bool FirmwareUpdate::scanFirmwareOnPCApp()
    {
        bool status = false;
        for( int i = 0; i < 6; i++ )
        {
            status = mt::USBFunctionFS::getUSBInstance().getStatus();
            if( status )
            {
                DBG( "iTS CONNENCTED" );
                return status;
            }
#if (defined( __linux__ ) || defined( __linux ) || defined( linux )) && !defined(KICK_DESKTOP)
            sleep( 1 );
#endif
        }
        DBG( "iTS not CONNENCTED" );
        return status;
    }

    std::vector<std::pair<std::string, mt::linuxFwInfo>> FirmwareUpdate::scanFirmwareOnUSBDrives()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;

        // Validate USB disk path
        if( !fs::exists( USBDISK_PATH ) || !fs::is_directory( USBDISK_PATH ) )
        {
            DBG( "Invalid USB disk path: " << USBDISK_PATH );
            return swuFiles;
        }

        // Get firmware files with version information from USB disk
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( USBDISK_PATH );

        // Check if no firmware files were found
        if( allFiles.empty() )
        {
            DBG( "No firmware files found on USB drives." );
            return swuFiles;
        }

        // Iterate through firmware files and compare versions
        for( const auto& file : allFiles )
        {
            try
            {
                // Compare the version of the firmware file
                mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );

                // Handle the result of the version comparison
                if( comparisonResult == mt::VersionComparisonResult::Less )
                {
                    DBG( "Found higher version: " << file.first );
                    swuFiles.push_back( file );
                }
                else if( comparisonResult == mt::VersionComparisonResult::Equal )
                {
                    DBG( "Already on updated version: " << file.first );
                }
                else
                {
                    DBG( "Found lower version: " << file.first );
                }
            } catch( const std::exception& e )
            {
                DBG( "Error comparing versions: " << e.what() );
                // Handle the error (e.g., continue iteration, log the error, or throw an exception)
            }
        }

        // Return the vector of firmware files with higher versions
        return std::move( swuFiles );
    }
    std::vector<std::pair<std::string, mt::linuxFwInfo>> FirmwareUpdate::scanFirmwareOnSDCards()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;

        // Validate USB disk path
        if( !fs::exists( SDCARD_PATH ) || !fs::is_directory( SDCARD_PATH ) )
        {
            DBG( "Invalid SDCARD disk path: " << SDCARD_PATH );
            return swuFiles;
        }

        // Get firmware files with version information from USB disk
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( SDCARD_PATH );

        // Check if no firmware files were found
        if( allFiles.empty() )
        {
            DBG( "No firmware files found on SDcard drives." );
            return swuFiles;
        }

        // Iterate through firmware files and compare versions
        for( const auto& file : allFiles )
        {
            try
            {
                // Compare the version of the firmware file
                mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );

                // Handle the result of the version comparison
                if( comparisonResult == mt::VersionComparisonResult::Less )
                {
                    DBG( "Found higher version: " << file.first );
                    swuFiles.push_back( file );
                }
                else if( comparisonResult == mt::VersionComparisonResult::Equal )
                {
                    DBG( "Already on updated version: " << file.first );
                }
                else
                {
                    DBG( "Found lower version: " << file.first );
                }
            } catch( const std::exception& e )
            {
                DBG( "Error comparing versions: " << e.what() );
                // Handle the error (e.g., continue iteration, log the error, or throw an exception)
            }
        }

        // Return the vector of firmware files with higher versions
        return std::move( swuFiles );
    }
#if 0
    std::vector<std::pair<std::string, mt::linuxFwInfo>> FirmwareUpdate::scanFirmwareOnUSBDrives()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( USBDISK_PATH );
        for( const auto& file : allFiles )
        {
            mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );
            if( comparisonResult == mt::VersionComparisonResult::Less )
            {
                DBG( "Got Higher Version so pushing to vector -> " << file.first );
                swuFiles.push_back( file );
            }
            else
            {
                DBG( "already on updated version" );
            }
        }
        return swuFiles;
    }

    std::vector<std::pair<std::string, linuxFwInfo>> FirmwareUpdate::scanFirmwareOnSDCards()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( SDCARD_PATH );
        for( const auto& file : allFiles )
        {
            mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );
            if( comparisonResult == mt::VersionComparisonResult::Less )
            {
                DBG( "Version is higher so pushing to vector -> " << file.first );
                swuFiles.push_back( file );
            }
            else
            {
                DBG( "already on updated version" );
            }
        }
        return swuFiles;
    }
#endif
}
