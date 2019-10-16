#include "remote-net-device.h"
#include "ns3/ethernet-header.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include <unistd.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FdNetDevice");

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

ssize_t RemoteNetDeviceFdReader::WriteClient (const uint8_t *buffer, size_t size) {
    int fd = _client.GetFd();
    if (fd < 0) {
        NS_LOG_ERROR("Client not ready.");
        return 0;
    }
    return write(fd, buffer, size);
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
        
        return tid;
}

RemoteNetDevice::RemoteNetDevice() :
_node (0), _node_id(0), _if_index(0), _mtu(1400), _reader(0), _server(0),
_port(0), _net(0), _link_up(false), _is_broadcast(true), _is_multicast(false), _start_ev(), _stop_ev()  {
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
    _link_callbacks();
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

    if (destination.IsBroadcast()) type = NS3_PACKET_BROADCAST;
    else if (destination == _address) type = NS3_PACKET_HOST;
    else if (destination.IsGroup()) type = NS3_PACKET_MULTICAST;
    else type = NS3_PACKET_OTHERHOST;

    _prx_callback(this, packet, protocol, source, destination, type);

    if (type == NS3_PACKET_HOST || type == NS3_PACKET_BROADCAST) {
        _rx_callback(this, packet, protocol, source);
    }
}

void RemoteNetDevice::SetIfIndex (const uint32_t index) {
    _if_index = index;
}

uint32_t RemoteNetDevice::GetIfIndex (void) const {
    return _if_index;
}

Ptr<Channel> RemoteNetDevice::GetChannel (void) const {
    return NULL;
}

void RemoteNetDevice::SetAddress (Address address) {
    _address = Mac48Address::ConvertFrom (address);
}

Address RemoteNetDevice::GetAddress (void) const {
    return _address;
}

bool RemoteNetDevice::SetMtu (const uint16_t mtu) {
    _mtu = mtu;
    return true;
}

uint16_t RemoteNetDevice::GetMtu (void) const {
    return _mtu;
}

bool RemoteNetDevice::IsLinkUp (void) const {
    return _link_up;
}

void RemoteNetDevice::AddLinkChangeCallback (Callback<void> callback) {
    _link_callbacks.ConnectWithoutContext(callback);
}

bool RemoteNetDevice::IsBroadcast (void) const {
    return _is_broadcast;
}

Address RemoteNetDevice::GetBroadcast (void) const {
    return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool RemoteNetDevice::IsMulticast (void) const {
    return _is_multicast;
}

Address RemoteNetDevice::GetMulticast (Ipv6Address addr) const {
    return Mac48Address::GetMulticast (addr);
}

Address RemoteNetDevice::GetMulticast (Ipv4Address group) const {
    return Mac48Address::GetMulticast (group);
}

bool RemoteNetDevice::IsPointToPoint (void) const {
    return false;
}

bool RemoteNetDevice::IsBridge (void) const {
    return false;
}

Ptr<Node> RemoteNetDevice::GetNode (void) const {
    return _node;
}

void RemoteNetDevice::SetNode (Ptr<Node> node) {
    _node = node;
}

bool RemoteNetDevice::NeedsArp (void) const {
    return true;
}

void RemoteNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb) {
    _rx_callback = cb;
}

void RemoteNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb) {
    _prx_callback = cb;
}

bool RemoteNetDevice::SupportsSendFrom () const {
    return true;
}

void RemoteNetDevice::SetIsBroadcast (bool broadcast) {
    _is_broadcast = broadcast;
}

void RemoteNetDevice::SetIsMulticast (bool multicast) {
    _is_multicast = multicast;
}

bool RemoteNetDevice::Send (Ptr<Packet> packet, const Address& dest, uint16_t protocol) {
    return SendFrom(packet, _address, dest, protocol);
}

bool RemoteNetDevice::SendFrom (Ptr<Packet> packet, const Address& src, const Address& dst, uint16_t protocol) {
    if (!_link_up) {
        NS_LOG_DEBUG("dropped received packet. (not up)");
        return false;
    }

    Mac48Address destination = Mac48Address::ConvertFrom (dst);
    Mac48Address source = Mac48Address::ConvertFrom (src);

    NS_LOG_LOGIC ("Transmit packet with UID " << packet->GetUid ());
    NS_LOG_LOGIC ("Transmit packet from " << source);
    NS_LOG_LOGIC ("Transmit packet to " << destination);

    EthernetHeader header (false);
    header.SetSource (source);
    header.SetDestination (destination);

    NS_ASSERT_MSG (packet->GetSize () <= _mtu, "Packet too big: " << packet->GetSize ());
    header.SetLengthType (protocol);
    packet->AddHeader (header);

    size_t len = (size_t) packet->GetSize ();
    uint8_t *buffer = (uint8_t*) malloc (len);
    packet->CopyData (buffer, len);

    ssize_t written = _reader->WriteClient(buffer, len);
    free (buffer); // TODO static alloc

    if (written < 0) {
        NS_LOG_ERROR("Error writing to client.");
        return false;
    }

    return true;
}

}

