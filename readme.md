RemoteNetDevice
---

An ns3 `NetDevice` that connects to a remote virtual switch in the real world ([distributor](https://github.com/Nat-Lab/distributor)). `RemoteNetDevice` allows you to connect multiple simulators or a regular Linux host to the simulation.

### Install

In ns3 source folder (where the `waf` is located), run the following commands:

```
$ git clone --recursive https://github.com/nat-lab/ns3-remote-net-device src/remote-net-device
$ ln -s src/remote-net-device/model/vendor build/vendor
```

### Usage

First, start a distribution server. Visit <https://github.com/Nat-Lab/distributor> for instruction on how to do that.

Then, you may create a `RemoteNetDevice` and attach it to a node like any other `NetDevice`. Assumes that your server is running on `127.0.0.1:9090` and you want to join network 1:

```c++
// create node and install internet stack
InternetStackHelper internet;
Ptr<Node> node = CreateObject<Node> ();
internet.Install (node);

// create device, and attach to node
Ptr<RemoteNetDevice> device = CreateObject<RemoteNetDevice> ();
device->SetRemote ("127.0.0.1", 9090, 1);
device->SetAddress (Mac48Address ("00:00:00:00:00:01"));
node->AddDevice (device);

// add ipv4 interface, etc.
Ptr<Ipv4> ipv4 = Node->GetObject<Ipv4> ();
int32_t iface_id = ipv4_router_1->AddInterface (device);
ipv4_router_1->AddAddress (iface_id, Ipv4InterfaceAddress("10.0.0.1", "/24"));
ipv4_router_1->SetMetric (iface_id, 1);
ipv4_router_1->SetUp (iface_id);
```

