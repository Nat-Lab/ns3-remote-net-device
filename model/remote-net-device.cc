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

TypeId RemoteNetDevice::GetTypeId () {
    static TypeId tid = TypeId ("ns3::RemoteNetDevice")
        .SetParent<NetDevice> ()
        .SetGroupName ("RemoteNetDevice")
        .AddConstructor<RemoteNetDevice> ()
        .AddAttribute (
            "Address",
            "The MAC address of this device.",
            Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
            MakeMac48AddressAccessor (&RemoteNetDevice::_address),
            MakeMac48AddressChecker ())
        .AddAttribute ("Start", "The simulation time at which to spin up the device thread.",
            TimeValue (Seconds (0.)),
            MakeTimeAccessor (&RemoteNetDevice::_start),
            MakeTimeChecker ())
        .AddAttribute ("Stop", "The simulation time at which to tear down the device thread.",
            TimeValue (Seconds (0.)),
            MakeTimeAccessor (&RemoteNetDevice::_stop),
            MakeTimeChecker ())
        .AddAttribute ("RxQueueSize", "Maximum size of the read queue.",
            UintegerValue (1024),
            MakeUintegerAccessor (&RemoteNetDevice::_queue_len),
            MakeUintegerChecker<uint32_t> ());
}

RemoteNetDevice::RemoteNetDevice() :
_node (0), _node_id(0), _if_index(0), _mtu(1400), _reader(0), _server(0),
_port(0), _net(0), _link_up(false), _is_broadcast(false), _start_ev(), _stop_ev()  {
    Start(_start);
}

}

