/*
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LoginRESTService_h__
#define LoginRESTService_h__

#include "DeadlineTimer.h"
#include "Define.h"
#include "IoContext.h"
#include "Login.pb.h"
#include "Session.h"
#include <atomic>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <mutex>
#include <thread>

class AsyncLoginRequest;
struct soap;
struct soap_plugin;

class LoginRESTService
{
public:
    LoginRESTService() : _ioContext(nullptr), _stopped(false), _port(0), _loginTicketCleanupTimer(nullptr) { }

    static LoginRESTService& Instance();

    bool Start(Trinity::Asio::IoContext* ioContext);
    void Stop();

    boost::asio::ip::tcp::endpoint const& GetAddressForClient(boost::asio::ip::address const& address) const;

    std::unique_ptr<Battlenet::Session::AccountInfo> VerifyLoginTicket(std::string const& id);

private:
    void Run();

    friend int32 handle_get_plugin(soap* soapClient);
    int32 HandleGet(soap* soapClient);

    friend int32 handle_post_plugin(soap* soapClient);
    int32 HandlePost(soap* soapClient);

    int32 SendResponse(soap* soapClient, google::protobuf::Message const& response);

    void HandleAsyncRequest(std::shared_ptr<AsyncLoginRequest> request);

    std::string CalculateShaPassHash(std::string const& name, std::string const& password);

    void AddLoginTicket(std::string const& id, std::unique_ptr<Battlenet::Session::AccountInfo> accountInfo);
    void CleanupLoginTickets();

    struct LoginTicket
    {
        // LoginTicket(std::string const& _id, std::unique_ptr<Battlenet::Session::AccountInfo> _accountInfo, std::time_t _ExpiryTime)
        // {
            // Id = std::move(_id);
            // Account = std::move(_accountInfo);
            // ExpiryTime = _ExpiryTime;
        // }

        std::string Id;
        std::unique_ptr<Battlenet::Session::AccountInfo> Account;
        std::time_t ExpiryTime;
    };

    struct ResponseCodePlugin
    {
        static char const* const PluginId;
        static int32 Init(soap* s, soap_plugin*, void*);
        static void Destroy(soap* s, soap_plugin* p);
        static int32 ChangeResponse(soap* s, int32 originalResponse, size_t contentLength);

        int32(*fresponse)(soap* s, int32 status, size_t length);
        int32 ErrorCode;
    };

    struct ContentTypePlugin
    {
        static char const* const PluginId;
        static int32 Init(soap* s, soap_plugin* p, void*);
        static void Destroy(soap* s, soap_plugin* p);
        static int32 OnSetHeader(soap* s, char const* key, char const* value);

        int32(*fposthdr)(soap* s, char const* key, char const* value);
        char const* ContentType;
    };

    Trinity::Asio::IoContext* _ioContext;
    std::thread _thread;
    std::atomic<bool> _stopped;
    Battlenet::JSON::Login::FormInputs _formInputs;
    std::string _bindIP;
    int32 _port;
    int32 _waitTime;
    std::array<boost::asio::ip::address, 2> _addresses;
    std::array<boost::asio::ip::tcp::endpoint, 2> _endpoints;
    std::mutex _loginTicketMutex;
    std::unordered_map<std::string, LoginTicket> _validLoginTickets;
    std::shared_ptr<Trinity::Asio::DeadlineTimer> _loginTicketCleanupTimer;
};

#define sLoginService LoginRESTService::Instance()

#endif // LoginRESTService_h__
