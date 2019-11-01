/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <json/json.h>
#include <boost/program_options.hpp>
#include <iostream>

#include "libServer/AddressChecksum.h"
#include "libUtils/JsonUtils.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

using namespace std;
namespace po = boost::program_options;

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

static std::string msg;

// Handlers
void on_open(client* c, websocketpp::connection_hdl hdl) {
  std::cout << "on_open" << endl;
  c->get_alog().write(websocketpp::log::alevel::app, "Send Message: " + msg);
  websocketpp::lib::error_code ec;
  c->send(std::move(hdl), msg, websocketpp::frame::opcode::text, ec);
  if (ec) {
    c->get_alog().write(websocketpp::log::alevel::app,
                        "Send Error: " + ec.message());
  }
}

void on_fail(client* c, websocketpp::connection_hdl hdl) {
  std::cout << "on_fail" << endl;
  (void)hdl;
  c->get_alog().write(websocketpp::log::alevel::app, "Connection Failed");
}

void on_message(client* c, websocketpp::connection_hdl hdl, const message_ptr& t_msg) {
  std::cout << "on_message" << endl;
  (void)hdl;
  c->get_alog().write(websocketpp::log::alevel::app,
                      "Received Reply: " + t_msg->get_payload());
}

void on_close(client* c, websocketpp::connection_hdl hdl) {
  std::cout << "on_close" << endl;
  (void)hdl;
  c->get_alog().write(websocketpp::log::alevel::app, "Connection Closed");
}

int main(int argc, const char* argv[]) {
  uint32_t option_;
  vector<string> addresses_;
  string url_;
  try {
    po::options_description desc("Options");
    desc.add_options()("help,h", "Print help messages")(
        "option,o", po::value<uint32_t>(&option_)->required(),
        "query option: 1 for NewBlock, 2 for EventLog")(
        "url,u", po::value<string>(&url_)->required(),
        "url for zilliqa websocket server")(
        "address,a", po::value<vector<string>>(&addresses_)->multitoken(),
        "multiple address supported, divide with space");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help")) {
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
    } catch (boost::program_options::required_option& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    string option;
    switch (option_) {
      case 1:
        option = "NewBlock";
        break;
      case 2:
        option = "EventLog";
        break;
      default:
        cerr << "Option not supported" << endl << endl;
        std::cout << desc;
        return ERROR_IN_COMMAND_LINE;
    }

    Json::Value j_query;

    j_query["query"] = option;

    if (option_ == 2) {
      if (addresses_.empty()) {
        std::cerr << "No address indicated" << endl << endl;
        std::cout << desc;
        return ERROR_IN_COMMAND_LINE;
      }

      for (const auto& address : addresses_) {
        j_query["addresses"].append(address);
      }
    }

    msg = JSONUtils::GetInstance().convertJsontoStr(j_query);

    client c;

    try {
      // set logging policy if needed
      // c.clear_access_channels(websocketpp::log::alevel::frame_header);
      c.set_access_channels(websocketpp::log::alevel::all);
      c.clear_access_channels(websocketpp::log::alevel::frame_payload);

      // Initialize ASIO
      c.init_asio();
      // c.set_reuse_addr(true);

      // Register our handler
      c.set_open_handler(bind(&on_open, &c, ::_1));
      c.set_fail_handler(bind(&on_fail, &c, ::_1));
      c.set_message_handler(bind(&on_message, &c, ::_1, ::_2));
      c.set_close_handler(bind(&on_close, &c, ::_1));

      // Create a connection to the given URI and queue it for connection once
      // the event loop starts
      websocketpp::lib::error_code ec;
      client::connection_ptr con = c.get_connection(url_, ec);
      if (ec) {
        std::cerr << "could not create connection becasue: " << ec.message()
                  << endl;
        return 0;
      }

      // websocketpp::connection_hdl hdl = con->get_handle();
      c.connect(con);

      // websocketpp::lib::thread asio_thread(&client::run, &c);
      // Start the ASIO io_service run loop
      c.run();
      c.stop();
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl << std::endl;
      return ERROR_UNHANDLED_EXCEPTION;
    } catch (websocketpp::lib::error_code e) {
      std::cerr << e.message() << std::endl << std::endl;
      return ERROR_UNHANDLED_EXCEPTION;
    } catch (...) {
      std::cerr << "other exception" << std::endl << std::endl;
      return ERROR_UNHANDLED_EXCEPTION;
    }
  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
}