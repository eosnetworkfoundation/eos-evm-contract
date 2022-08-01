#include "core_system_plugin.hpp"
#include "channels.hpp"

class core_system_plugin_impl : std::enable_shared_from_this<core_system_plugin_impl> {
   public:
      core_system_plugin_impl()
        : shutdown_event(appbase::app().get_channel<channels::shutdown_signal>()){}

      inline void init() {
         boost::asio::io_context& sched = appbase::app().get_io_service();
         boost::asio::signal_set sigs{sched, SIGINT, SIGTERM};

         SILK_DEBUG << "Signals registered on scheduler " << &sched;

         const auto& handler = [&](const boost::system::error_code& ec, int sig) {
            SILK_DEBUG << "Signal caught " << sig;
            shutdown();
         };
         sigs.async_wait(handler);
      }

      inline void shutdown() {
         shutdown_event.publish(90, std::make_shared<channels::shutdown>());
      }

      channels::shutdown_signal::channel_type& shutdown_event;
};

core_system_plugin::core_system_plugin() : my(new core_system_plugin_impl()) {}
core_system_plugin::~core_system_plugin() {}

void core_system_plugin::set_program_options( appbase::options_description& cli, appbase::options_description& cfg ) {
}

void core_system_plugin::plugin_initialize( const appbase::variables_map& options ) {
   my->init();
   SILK_INFO << "Initialized core_system Plugin";
}

void core_system_plugin::plugin_startup() {
}

void core_system_plugin::plugin_shutdown() {
   my->shutdown();
   SILK_INFO << "Shutdown core_system plugin";
}