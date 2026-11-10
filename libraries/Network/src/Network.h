/* IPv4 networking on lwIP, with NetworkClient, NetworkServer, NetworkUDP
 * and MDNS. Ethernet provides the CH32V407/467 MAC/PHY interface.
 * Call Network.poll() from loop() to service frames and timeouts. */
#pragma once
#include "NetworkClient.h"
#include "NetworkEthernet.h"
#include "NetworkInterface.h"
#include "NetworkManager.h"
#include "NetworkMdns.h"
#include "NetworkServer.h"
#include "NetworkUdp.h"
