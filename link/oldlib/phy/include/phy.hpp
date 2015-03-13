#pragma once

/**
 * @file phy.hpp
 * @brief Declares the interface to a physical device driver within netlink.
 */

#include <vector>

namespace fos
{
    namespace net
    {
        class Link
        {
        public:
            /**
             * @brief This is an upcall to the link layer when data is received.
             *
             * The device driver is expected to install its own idle
             * handlers or message callbacks with the dispatch library
             * to ensure that this function is called when data is
             * received.
             */
            virtual void incoming(void * msg, size_t size) =0;
        };
        
        class Phy
        {
        public:
            virtual ~Phy() {};

            /**
             * @brief Send data out on this device.
             */
            virtual FosStatus send(
                void * in_data,
                size_t in_size) = 0;

            /**
             * @brief Stop sending data up to the link layer.
             *
             * Called when the link layer is saturated and needs to apply back pressure.
             */
            virtual void pause() = 0;

            /**
             * @brief Resume normal operation
             *
             * @see pause
             */
            virtual void unpause() = 0;

            /**
             * @brief Notify parent process that this device has started and
             * send capabilities out to talk to this device.
             *
             */
            virtual void notifyStarted() = 0;

            /**
             * @brief A vector of physical devices.
             */
            typedef std::vector<Phy*> Vector;

            virtual void quit() = 0;

        protected:
            Phy(Link & link) : m_link(link) {};

            Link & m_link;
        };
    }
}

