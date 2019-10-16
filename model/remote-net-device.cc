#include "remote-net-device.h"
#include "ns3/uinteger.h"
#include <unistd.h>

namespace ns3 {

RemoteNetDeviceFdReader::RemoteNetDeviceFdReader(in_addr_t server_addr, in_port_t port, distributor::net_t net) :
_client(server_addr, port, net) {}

void RemoteNetDeviceFdReader::StartClient () {
    _client.Start();
}

void RemoteNetDeviceFdReader::StopClient () {
    _client.Stop();
}

FdReader::Data RemoteNetDeviceFdReader::DoRead (void) {
    int fd = _client.GetFd();
    if (fd < 0) {
        NS_FATAL_ERROR("Failed to get FD from DistClient.");
        return FdReader::Data (NULL, 0);
    }

    uint8_t *buf = (uint8_t *) malloc (DIST_CLIENT_BUF_SZ);
    NS_ABORT_MSG_IF (buf == 0, "malloc() failed.");
    ssize_t len = read(fd, buf, DIST_CLIENT_BUF_SZ);
    if (len < 0) {
        free(buf);
        buf = 0;
        len = 0;
    }
    return FdReader::Data (buf, len);
}


}

