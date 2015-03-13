#include "./phy.hpp"

#include <pthread.h>
#include <messaging/messaging.hpp>

namespace fos
{
    namespace net
    {
        namespace phy
        {
            class Eth : public Phy
            {
            public:
                Eth(Link & link);
                ~Eth();

                /**
                 * @brief Send data out on this device.
                 */
                Status send(
                    void * in_data,
                    size_t in_size);

                void quit();
                void pause();
                void unpause();
                void notifyStarted();

                MacAddress getMacAddress() const;

                struct netfront_dev *getDevId() const;

                messaging::Mailbox * getOutMailbox() { return m_out_mbox; }

            private:
                /**
                 * @brief Fill in the dev structure for uk.
                 */
                Status populateDev();

                /**
                 * @brief Incoming data from uk.
                 */
                messaging::Mailbox m_mbox;
                messaging::Alias m_alias;
                messaging::Mailbox::Capability m_mbox_cap;

                messaging::Mailbox * m_out_mbox;
                messaging::Alias m_out_alias;
                messaging::Mailbox::Capability m_out_mbox_cap;

                pthread_t m_outgoing_thread;

                /**
                 * @brief Structure to store shared info with uk.
                 */
                struct netfront_dev * m_dev;

                /**
                 * @brief Handler for incoming data.
                 */
                static void incoming(void * msg, size_t size, void * vpeth);
            };
        }
    }
}
