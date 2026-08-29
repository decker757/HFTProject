#pragma once

#include <string>
#include <string.h>
#include <libpq-fe.h>
#include <stdexcept>

#include "Trade.h"

class MarketDataStore {
private:
    PGconn* m_conn;
    std::string m_host;
    int m_port;
    std::string m_dbName;
    std::string m_user;
    std::string m_password;
    bool m_isConnected;

public:
    MarketDataStore(std::string host, int port, std::string dbName, std::string user, std::string password);
    ~MarketDataStore();
    void updateDB(Trade t);
};