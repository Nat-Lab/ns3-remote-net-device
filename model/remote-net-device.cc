#include "remote-net-device.h"
#include "ns3/ethernet-header.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include <unistd.h>

namespace ns3 {

RemoteNetDeviceFdReader::RemoteNetDeviceFdReader(in_addr_t server_addr, in_port_t port, distributor::net_t net) :
_client(server_addr, port, net) {}

void RemoteNetDeviceFdReader::StartClient (Callback<void, uint8_t *, ssize_t> cb) {
    _client.Start();
    Start(_client.GetFd(), cb);
}

void RemoteNetDeviceFdReader::StopClient () {
    Stop();
    _client.Stop();
    _client.Join();
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

RemoteNetDevice::~RemoteNetDevice() {
    CriticalSection cs (_queue_mtx);

    while (!_queue.empty()) {
        std::pair<uint8_t*, ssize_t> nxt = _queue.front();
        _queue.pop();
        free(nxt.first);
    }
}

void RemoteNetDevice::DoDispose (void) {
    StopDevice();
    NetDevice::DoDispose();
}

void RemoteNetDevice::Start (Time start) {
    Simulator::Cancel (_start_ev);
    _start_ev = Simulator::Schedule (start, &RemoteNetDevice::StartDevice, this);

}

void RemoteNetDevice::Stop (Time stop) {
    Simulator::Cancel (_stop_ev);
    _stop_ev = Simulator::Schedule (stop, &RemoteNetDevice::StopDevice, this);
}

void RemoteNetDevice::SetRemote (in_addr_t server_addr, in_port_t port, distributor::net_t net) {
    _server = server_addr;
    _port = port;
    _net = net;
}

void RemoteNetDevice::StartDevice () {
    if (_port == 0 || _server == 0) {
        NS_LOG_ERROR("Server invalid.");
        return;
    }

    _node_id = GetNode()->GetId();
    _reader = Create<RemoteNetDeviceFdReader> (_server, _port, _net);
    _reader->StartClient(MakeCallback (&RemoteNetDevice::ReceiveCallback, this));

    _link_up = true;
}

void RemoteNetDevice::StopDevice () {
    if (_reader != 0) {
        _reader->StopClient();
        _reader = 0;
    }
}

void RemoteNetDevice::ReceiveCallback (uint8_t *buf, ssize_t len) {
    if(_queue.size() > _queue_len) {
        NS_LOG_WARN("Queue full.");
        return;
    }

    _queue_mtx.Lock();
    _queue.push(std::make_pair (buf, len));
    _queue_mtx.Unlock();
    Simulator::ScheduleWithContext (_node_id, Time(0), MakeEvent (&RemoteNetDevice::ForwardUp, this));
}

void RemoteNetDevice::ForwardUp () {
    uint8_t *buf = nullptr; 
    ssize_t len = 0;

    _queue_mtx.Lock();
    std::pair<uint8_t *, ssize_t> next = _queue.front();
    _queue.pop();
    buf = next.first;
    len = next.second;
    _queue_mtx.Unlock();

    if (buf == nullptr || len == 0) {
        NS_LOG_ERROR("Can't forward up: bad packet in queue.\n");
        return;
    }

    // copy to ns3-packet
    Ptr<Packet> packet = Create<Packet> (reinterpret_cast<const uint8_t *> (buf), len);
    free (buf);

    EthernetHeader header (false);
    if (packet->GetSize () < header.GetSerializedSize ()) {
        NS_LOG_ERROR("Bad ethernet frame.\n");
    }

    packet->RemoveHeader (header);

    // header fields
    Mac48Address destination = header.GetDestination();
    Mac48Address source = header.GetSource();
    uint16_t protocol = header.GetLengthType ();

    PacketType type;
    if (destination.IsGroup()) {
        NS_LOG_WARN("Multicast not implemented.");
        return;
    }

    if (destination.IsBroadcast()) type = NS3_PACKET_BROADCAST;
    else if (destination == _address) type = NS3_PACKET_HOST;
    else type = NS3_PACKET_OTHERHOST;


    _prx_callback(this, packet, protocol, source, destination, type);

    if (type == NS3_PACKET_HOST || type == NS3_PACKET_BROADCAST) {
        _rx_callback(this, packet, protocol, source);
    }
}

}

