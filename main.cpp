#include <PvDevice.h>
#include <PvDeviceGEV.h>
#include <PvStream.h>
#include <PvStreamGEV.h>
#include <PvBuffer.h>
#include <PvBufferWriter.h>
#include <PvSystem.h>
#include <list>
#include <stdexcept>
#include <vector>
#include <unistd.h>
#include <sstream>

#define BUFFER_COUNT (16)
typedef std::list<PvBuffer *> BufferList;

#define AUTOFOCUS_CMD "AutoFocus"
#define ACQUISITION_START_CMD "AcquisitionStart"
#define ACQUISITION_STOP_CMD "AcquisitionStop"
#define ACQUISITION_RATE_CMD "AcquisitionRate"
#define BANDWIDTH_CMD "Bandwidth"


class CameraException: virtual public std::exception {

protected:
    std::runtime_error error; // Error message

public:
    /** Constructor (C++ STL string).
     *  @param message The error message
     */
    explicit CameraException(const std::string &message) :
        error(message) {};

    ~CameraException() override = default;

    /** Returns a pointer to the (constant) error description.
     *  @return A pointer to a const char*. The underlying memory
     *  is in possession of the Except object. Callers must
     *  not attempt to free the memory.
     */
    const char* what() const noexcept override {
        return error.what();
    }
};

bool checkDeviceInfo(const PvDeviceInfo* deviceInfo, const std::string& ipAddress = "") {
    if (nullptr == deviceInfo) {
        throw CameraException("Unable to check mDevice info: 'mDeviceInfo' is null.");
    }

    if (deviceInfo->IsConfigurationValid()) {
        if (!ipAddress.empty()) {
            auto dig = dynamic_cast<const PvDeviceInfoGEV *>(deviceInfo);
            if (dig->GetIPAddress().GetAscii() == ipAddress) {
                return true;
            }
            return false;
        }
        return true;
    }
    return false;
}

const PvDeviceInfo* scanDevices(PvSystem *pvSystem, const std::string& ipAddress = "") {

    if (nullptr == pvSystem) {
        throw CameraException("Unable to scan devices: 'mSystem' is null.");
    }

    pvSystem->Find();
    // Detect, select mDevice.
    std::vector<const PvDeviceInfo *> devicesInfos;
    for (int i = 0; i < pvSystem->GetInterfaceCount(); i++) {
        const auto *interface = dynamic_cast<const PvInterface *>(pvSystem->GetInterface(i));
        if (interface != nullptr) {
            for (int j = 0; j < interface->GetDeviceCount(); j++ ) {
                const auto *device_info = dynamic_cast<const PvDeviceInfo *>(interface->GetDeviceInfo(j));
                if (device_info != nullptr){
                    devicesInfos.push_back(device_info);
                }
            }
        }
    }

    if(devicesInfos.empty()){
        throw CameraException("Unable to connect to mDevice: 'devicesInfos' is empty.");
    }

    if (!ipAddress.empty()) {
        for (auto & di : devicesInfos) {
            auto *dig = dynamic_cast<const PvDeviceInfoGEV*>(di);
            if (dig != nullptr) {
                if (dig->GetIPAddress().GetAscii() == ipAddress) {
                    return di;
                }
            }
        }
    }

    return devicesInfos[0];
}

const PvDeviceInfo* findDevice(
    PvSystem *pvSystem,
    const PvDeviceInfo* deviceInfo,
    const std::string& ipAddress = "",
    int maxRetries = 10,
    int waitTime = 1
) {
    if (nullptr == pvSystem) {
        throw CameraException("Unable to connect to mDevice: 'mSystem' is null.");
    }

    if (nullptr != deviceInfo && checkDeviceInfo(deviceInfo, ipAddress)) {
        return deviceInfo;
    }

    // The mDevice is selected
    auto _device_info = scanDevices(pvSystem, ipAddress);
    // If the IP Address valid?
    if (checkDeviceInfo(_device_info, ipAddress)) {
        return _device_info;
    }

    if (ipAddress.empty()) {
        throw CameraException("Device does not have a valid IP and it were not provided");
    }

    auto device_info_gev = dynamic_cast<const PvDeviceInfoGEV *>(_device_info);
    if (device_info_gev != nullptr) {
        // Force new IP address.
        PvResult result = PvDeviceGEV::SetIPConfiguration(
            device_info_gev->GetMACAddress().GetAscii(),
            ipAddress.c_str(),
            device_info_gev->GetSubnetMask().GetAscii(),
            device_info_gev->GetDefaultGateway().GetAscii()
        );
        if (!result.IsOK()) {
            throw CameraException("Unable to configure mDevice IP address");
        }
    } else {
        throw CameraException("Unable to configure mDevice IP address");
    }

    for (int i = 0; i < maxRetries; i++) {
        _device_info = scanDevices(pvSystem, ipAddress);
        if (checkDeviceInfo(_device_info, ipAddress)) {
            return _device_info;
        }
        sleep(waitTime);
    }

    throw CameraException("Unable to configure mDevice IP address");
}

PvDevice* connectToDevice(const PvDeviceInfo* deviceInfo) {
    if (nullptr == deviceInfo) {
        throw CameraException("Unable to connect to pPvDevice: 'mDeviceInfo' is null.");
    }

    PvResult result;
    // Connect to the GigE Vision or USB3 Vision pPvDevice
    PvDevice *pPvDevice = PvDevice::CreateAndConnect(deviceInfo, &result);
    if (pPvDevice == nullptr) {
        std::stringstream message;
        message << "Unable to connect to pPvDevice " << deviceInfo->GetDisplayID().GetAscii();
        throw CameraException(message.str());
    }

    return pPvDevice;
}

PvStream* openStream(const PvDeviceInfo* deviceInfo) {

    if (nullptr == deviceInfo) {
        throw CameraException("Unable to connect to mDevice: 'mDeviceInfo' is null.");
    }

    PvResult result;
    // Open mStream to the GigE Vision or USB3 Vision mDevice
    PvStream* stream = PvStream::CreateAndOpen(deviceInfo->GetConnectionID(), &result);
    if (stream == nullptr) {
        std::stringstream message;
        message << "Unable to create mStream from " << deviceInfo->GetDisplayID().GetAscii();
        throw CameraException(message.str());
    }

    return stream;
}

void configStream(PvDevice* device, PvStream *stream) {

    if (nullptr == device) {
        throw CameraException("Unable to create mStream buffers: 'mDevice' is null.");
    }
    if (nullptr == stream) {
        throw CameraException("Unable to create mStream buffers: 'mStream' is null.");
    }

    // If this is a GigE Vision mDevice, configure GigE Vision specific streaming parameters
    auto *deviceGev = dynamic_cast<PvDeviceGEV *>(device);
    if (nullptr == deviceGev) {
        throw CameraException("Unable to configure mStream: 'deviceGev' is null.");
    } else {
        auto *streamGev = dynamic_cast<PvStreamGEV *>(stream);
        // Negotiate packet size
        deviceGev->NegotiatePacketSize();
        // Configure mDevice streaming destination
        deviceGev->SetStreamDestination(streamGev->GetLocalIPAddress(), streamGev->GetLocalPort());
    }
}

void createStreamBuffers(PvDevice* device, PvStream *stream, BufferList* buffers) {

    if (nullptr == device) {
        throw CameraException("Unable to create mStream buffers: 'mDevice' is null.");
    }
    if (nullptr == stream) {
        throw CameraException("Unable to create mStream buffers: 'mStream' is null.");
    }

    // Reading payload size from mDevice
    uint32_t payloadSize = device->GetPayloadSize();

    // Use BUFFER_COUNT or the maximum number of buffers, whichever is smaller
    uint32_t maxBufferCount = stream->GetQueuedBufferMaximum();
    uint32_t bufferCount = (maxBufferCount < BUFFER_COUNT) ? maxBufferCount : BUFFER_COUNT;

    // Allocate buffers
    for (uint32_t i = 0; i < bufferCount; i++) {
        // Create new buffer object
        auto *buffer = new PvBuffer;
        // Have the new buffer object allocate payload memory
        buffer->Alloc(static_cast<uint32_t>( payloadSize ));
        // Add to external list - used to eventually release the buffers
        buffers->push_back(buffer);
    }

    // Queue all buffers in the mStream
    auto it = buffers->begin();
    while (it != buffers->end()) {
        stream->QueueBuffer(*it);
        it++;
    }
}

void clearStreamBuffers(PvStream *stream, BufferList* buffers) {

    // Abort all buffers from the mStream and dequeue
    stream->AbortQueuedBuffers();
    while (stream->GetQueuedBufferCount() > 0) {
        PvBuffer *clear_buffer = nullptr;
        PvResult lOperationResult;
        stream->RetrieveBuffer(&clear_buffer, &lOperationResult);
    }

    // Go through the buffer list and delete it
    auto lIt = buffers->begin();
    while (lIt != buffers->end()) {
        delete *lIt;
        lIt++;
    }
    // Clear the buffer list
    buffers->clear();
}

void freeCameraResources(PvDevice* device, PvStream *stream) {

    // Close mStream
    stream->Close();
    PvStream::Free(stream);

    // Disconnect mDevice
    device->Disconnect();
    PvDevice::Free(device);
}

void acquireImage(PvDevice* device, PvStream *stream, const std::string& out_path) {

    if (nullptr == device) {
        throw CameraException("Unable to create mStream buffers: 'mDevice' is null.");
    }
    if (nullptr == stream) {
        throw CameraException("Unable to create mStream buffers: 'mStream' is null.");
    }

    // Get mDevice parameters need to control streaming
    auto *deviceParams = device->GetParameters();

    // Map the GenICam AcquisitionStart and AcquisitionStop commands
    auto *startCmd = dynamic_cast<PvGenCommand *>(deviceParams->Get(ACQUISITION_START_CMD));
    auto *stopCmd = dynamic_cast<PvGenCommand *>(deviceParams->Get(ACQUISITION_STOP_CMD));

    // Get mStream parameters
    PvGenParameterArray *streamParams = stream->GetParameters();

    // Map a few GenICam mStream stats counters
    auto *frameRate = dynamic_cast<PvGenFloat *>(streamParams->Get(ACQUISITION_RATE_CMD));
    auto *bandwidth = dynamic_cast<PvGenFloat *>(streamParams->Get(BANDWIDTH_CMD));

    // Enable streaming and send the AcquisitionStart command
    device->StreamEnable();
    startCmd->Execute();

    double frameRateVal = 0.0;
    double bandwidthVal = 0.0;

    // Acquire images until the user instructs us to stop.

    PvBuffer *buffer = nullptr;
    PvResult operationResult;

    // Retrieve next buffer
    PvResult result = stream->RetrieveBuffer(&buffer, &operationResult, 1000);
    if (result.IsOK()) {
        if (operationResult.IsOK()) {
            PvPayloadType payloadType;
            // We now have a valid buffer. This is where you would typically process the buffer.
            frameRate->GetValue(frameRateVal);
            bandwidth->GetValue(bandwidthVal);

            // If the buffer contains an image, display width and height.
            // uint32_t imgWidth, imgHeight;
            payloadType = buffer->GetPayloadType();

            if (payloadType == PvPayloadTypeImage) {
                // Get image specific buffer interface.
                PvImage *lImage = buffer->GetImage();
                // Read width, height.
                // imgWidth = lImage->GetWidth();
                // imgHeight = lImage->GetHeight();
                if (buffer->GetBlockID() != 0)
                {
                    PvBufferWriter bufferWriter;
                    bufferWriter.Store(buffer, PvString(out_path.c_str()), PvBufferFormatTIFF);
                }
            } else {
                throw CameraException("Buffer does not contain an image");
            }
        } else {
            std::stringstream message;
            message << "Buffer retrieve operation error: " << operationResult.GetCodeString().GetAscii();
            throw CameraException(message.str());
        }
        // Re-queue the buffer in the mStream object
        stream->QueueBuffer(buffer);
    } else {
        std::stringstream message;
        message << "Buffer failure: " << result.GetCodeString().GetAscii();
        throw CameraException(message.str());
    }

    // Tell the mDevice to stop sending images.
    stopCmd->Execute();

    // Disable streaming on the mDevice
    device->StreamDisable();
}

int main(int argc, char *argv[])
{
    auto *pvSystem = new PvSystem;
    const PvDeviceInfo *deviceInfo = nullptr;
    PvDevice *device;
    PvStream *stream;
    auto *buffers = new BufferList;
    std::string ipAddress = "192.168.1.140";

    // Setup connection
    deviceInfo = findDevice(pvSystem, deviceInfo, ipAddress);
    device = connectToDevice(deviceInfo);
    stream = openStream(deviceInfo);
    configStream(device, stream);

    // Acquire image
    createStreamBuffers(device, stream, buffers);
    acquireImage(device, stream, "image.tiff");
    clearStreamBuffers(stream, buffers);

    freeCameraResources(device, stream);

    return 0;
}
