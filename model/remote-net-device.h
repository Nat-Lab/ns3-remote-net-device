#ifndef REMOTE_NET_DEVICE_H
#define REMOTE_NET_DEVICE_H
#include <queue>
#include "ns3/net-device.h"
#include "ns3/unix-fd-reader.h"
#include "ns3/system-mutex.h"
#include "ns3/simulator.h"
#include "ns3/traced-callback.h"
#include "vendor/distributor/src/types.h"
#include "vendor/distributor/src/fd-client.h"

namespace ns3 {

class RemoteNetDeviceFdReader : public FdReader {
public:
    RemoteNetDeviceFdReader (in_addr_t server_addr, in_port_t port, distributor::net_t net);

    void StartClient(Callback<void, uint8_t *, ssize_t> cb);
    void StopClient();
    ssize_t WriteClient(const uint8_t *buffer, size_t size);

private:
    distributor::FdClient _client;
    FdReader::Data DoRead (void);
};

class RemoteNetDevice : public NetDevice {
public:
    static TypeId GetTypeId ();

    RemoteNetDevice();
    virtual ~RemoteNetDevice ();

    void Start (Time start);
    void Stop (Time stop);

    void SetRemote (in_addr_t server_addr, in_port_t port, distributor::net_t net);

    // inherited
    virtual void SetIfIndex (const uint32_t index);
    virtual uint32_t GetIfIndex (void) const;
    virtual Ptr<Channel> GetChannel (void) const;
    virtual void SetAddress (Address address);
    virtual Address GetAddress (void) const;
    virtual bool SetMtu (const uint16_t mtu);
    virtual uint16_t GetMtu (void) const;
    virtual bool IsLinkUp (void) const;
    virtual void AddLinkChangeCallback (Callback<void> callback);
    virtual bool IsBroadcast (void) const;
    virtual Address GetBroadcast (void) const;
    virtual bool IsMulticast (void) const;
    virtual Address GetMulticast (Ipv4Address group) const;
    virtual Address GetMulticast (Ipv6Address addr) const;
    virtual bool IsPointToPoint (void) const;
    virtual bool IsBridge (void) const;
    virtual Ptr<Node> GetNode (void) const;
    virtual void SetNode (Ptr<Node> node);
    virtual bool NeedsArp (void) const;
    virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
    virtual void SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb);
    virtual bool SupportsSendFrom () const;
    virtual void SetIsBroadcast (bool broadcast);
    virtual void SetIsMulticast (bool multicast);

    virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocol);
    virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocol);

protected:
    virtual void DoDispose (void);

private:
    RemoteNetDevice (RemoteNetDevice &); // trap copy constructor
    void StartDevice ();
    void StopDevice ();
    void ReceiveCallback (uint8_t *buf, ssize_t len);
    void ForwardUp ();

    Ptr<Node> _node;
    uint32_t _node_id;
    uint32_t _if_index;
    uint16_t _mtu;

    in_addr_t _server;
    in_port_t _port;
    distributor::net_t _net;

    Ptr<RemoteNetDeviceFdReader> _reader;
    Mac48Address _address;
    bool _link_up;
    bool _is_broadcast;
    bool _is_multicast;
    std::queue<std::pair<uint8_t*, ssize_t>> _queue;
    uint32_t _queue_len;
    SystemMutex _queue_mtx;

    Time _start;
    Time _stop;
    EventId _start_ev;
    EventId _stop_ev;

    NetDevice::ReceiveCallback _rx_callback;
    NetDevice::PromiscReceiveCallback _prx_callback;
    TracedCallback<> _link_callbacks;
};

}

#endif /* REMOTE_NET_DEVICE_H */

