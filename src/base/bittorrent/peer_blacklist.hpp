#pragma once

#include <regex>

#include <libtorrent/torrent_info.hpp>

#include <QHostAddress>

#include "base/net/geoipmanager.h"

#include "peer_filter_plugin.hpp"
#include "peer_logger.hpp"

// bad peer filter
bool is_bad_peer(const lt::peer_info& info)
{
  std::regex id_filter("-(XL|SD|XF|QD|BN|DL)(\\d+)-");
  std::regex ua_filter(R"((\d+.\d+.\d+.\d+|cacao_torrent))");
  return std::regex_match(info.pid.data(), info.pid.data() + 8, id_filter) || std::regex_match(info.client, ua_filter);
}

// Unknown Peer filter
bool is_unknown_peer(const lt::peer_info& info)
{
  unsigned short port = info.ip.port();
  return port >= 65000 || info.client.find("Unknown") != std::string::npos;
}

// Offline Downloader filter
bool is_offline_downloader(const lt::peer_info& info)
{
  QString country = Net::GeoIPManager::instance()->lookup(QHostAddress(info.ip.data()));
  return country != QLatin1String("CN");
}

// BitTorrent Media Player Peer filter
bool is_bittorrent_media_player(const lt::peer_info& info)
{
  std::regex wl_filter("-(qB|TR3|UT)(\\S+)-");
  return !std::regex_match(info.pid.data(), info.pid.data() + 8, wl_filter);
}


// drop connection action
void drop_connection(lt::peer_connection_handle ph)
{
  ph.disconnect(boost::asio::error::connection_refused, lt::operation_t::bittorrent, lt::disconnect_severity_t{0});
}


template<typename F>
auto wrap_filter(F filter, const std::string& tag)
{
  return [=](const lt::peer_info& info, bool handshake, bool* stop_filtering) {
    bool matched = filter(info);
    *stop_filtering = !handshake && !matched;
    if (matched)
      peer_logger_singleton::instance().log_peer(info, tag);
    return matched;
  };
}


std::shared_ptr<lt::torrent_plugin> create_peer_action_plugin(
    const lt::torrent_handle& th,
    filter_function filter,
    action_function action)
{
  // ignore private torrents
  if (th.torrent_file() && th.torrent_file()->priv())
    return nullptr;

  return std::make_shared<peer_action_plugin>(std::move(filter), std::move(action));
}


// plugins factory functions

std::shared_ptr<lt::torrent_plugin> create_drop_bad_peers_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_bad_peer, "bad peer"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_drop_unknown_peers_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_unknown_peer, "unknown peer"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_drop_offline_downloader_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_offline_downloader, "offline downloader"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_drop_bittorrent_media_player_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_bittorrent_media_player, "bittorrent media player"), drop_connection);
}
