#pragma once
#include <messaging/messaging.hpp>
#include <sys/net/link.h>

// Loopback device
namespace fos { namespace net {
namespace transport {

    class Loopback
    {
        public:
            Loopback();
            ~Loopback();

            Status send(void *data, int len);
            Status sendV(struct iovec * in_iov, size_t in_iovcnt);

            void quit();
            void notifyStarted();

            static Loopback & loopback() { return *m_loopback; }

        private:

            static Loopback * m_loopback;
    };

}}}

extern "C" {
err_t loopif_init(struct netif *loopif);
err_t loopif_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr);
err_t loopif_low_level_output(struct netif *netif, struct pbuf *p);
}
