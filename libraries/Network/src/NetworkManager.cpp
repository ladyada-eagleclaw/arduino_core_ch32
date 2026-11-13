#include "NetworkManager.h"
#include "internal/NetworkCore.h"

#include <string.h>

#include "lwip/dns.h"
#include "lwip/netif.h"

NetworkManager Network;

static char default_hostname[33];

bool NetworkManager::begin() {
  ch32rv_net::start();
  return true;
}

namespace {
struct Lookup {
  bool done;
  bool found;
  bool waiting;
  uint32_t ip;
};
} // namespace

static void dns_found_cb(const char *name, const ip_addr_t *ip, void *arg) {
  (void)name;
  auto *l = static_cast<Lookup *>(arg);
  l->done = true;
  l->found = ip != nullptr;
  l->ip = ip ? ip_addr_get_ip4_u32(ip) : 0;
  if (!l->waiting) {
    free(l);
  }
}

int NetworkManager::hostByName(const char *aHostname, IPAddress &aResult,
                               uint32_t timeoutMs) {
  ch32rv_net::start();
  if (!aHostname || !*aHostname) {
    return 0;
  }
  if (aResult.fromString(aHostname)) {
    return 1;
  }
  /* The callback may come after a timeout gave up: the lookup lives as long
   * as lwIP can still answer it (DNS_MAX_RETRIES x the retry interval). */
  // Give each request its own state: an earlier timed-out request may still
  // complete while a later lookup is waiting for a different host name.
  Lookup *lookup = (Lookup *)calloc(1, sizeof(Lookup));
  if (!lookup) {
    return 0;
  }
  lookup->waiting = true;
  ip_addr_t ip;
  const err_t err = dns_gethostbyname(aHostname, &ip, dns_found_cb, lookup);
  if (err == ERR_OK) {
    aResult = IPAddress(ip_addr_get_ip4_u32(&ip));
    free(lookup);
    return 1;
  }
  if (err != ERR_INPROGRESS) {
    free(lookup);
    return 0;
  }
  ch32rv_net::waitFor([](void *a) { return static_cast<Lookup *>(a)->done; },
                      lookup, timeoutMs);
  if (!lookup->done) {
    lookup->waiting = false; // The eventual callback releases the request.
    return 0;
  }
  bool found = lookup->found;
  if (found) {
    aResult = IPAddress(lookup->ip);
  }
  free(lookup);
  return found ? 1 : 0;
}

uint8_t *NetworkManager::macAddress(uint8_t *mac) {
  NetworkInterface *ifc = getDefaultInterface();
  if (ifc) {
    return ifc->macAddress(mac);
  }
  memset(mac, 0, 6);
  return mac;
}

String NetworkManager::macAddress() {
  NetworkInterface *ifc = getDefaultInterface();
  return ifc ? ifc->macAddress() : String("00:00:00:00:00:00");
}

bool NetworkManager::setDefaultInterface(NetworkInterface &ifc) {
  return ifc.setDefault();
}

NetworkInterface *NetworkManager::getDefaultInterface() {
  ch32rv_net::start();
  return netif_default ? static_cast<NetworkInterface *>(netif_default->state)
                       : nullptr;
}

bool NetworkManager::isOnline() {
  ch32rv_net::start();
  struct netif *n;
  NETIF_FOREACH(n) {
    auto *ifc = static_cast<NetworkInterface *>(n->state);
    if (ifc && ifc->hasIP()) {
      return true;
    }
  }
  return false;
}

size_t NetworkManager::printTo(Print &out) const {
  size_t n = 0;
  struct netif *i;
  NETIF_FOREACH(i) {
    auto *ifc = static_cast<NetworkInterface *>(i->state);
    if (ifc) {
      n += out.println(*ifc);
    }
  }
  return n;
}

const char *NetworkManager::getHostname() { return default_hostname; }

bool NetworkManager::setHostname(const char *hostname) {
  if (!hostname || strlen(hostname) >= sizeof default_hostname) {
    return false;
  }
  strcpy(default_hostname, hostname);
  return true;
}

extern "C" void ch32rv_net_task(void);
void NetworkManager::poll() { ch32rv_net_task(); }
