#include "market_data/MarketDataStore.h"
#include <libpq-fe.h>
#include <string.h>

MarketDataStore::MarketDataStore(std::string host, int port, std::string dbName, std::string user, std::string password) : 
      m_host(host), 
      m_port(port), 
      m_dbName(dbName), 
      m_user(user), 
      m_password(password), 
      m_conn(nullptr), 
      m_isConnected(false) 
    {
        std::string connStr = "host=" + m_host +
                              " port=" + std::to_string(m_port) +
                              " dbname=" + m_dbName +
                              " user=" + m_user +
                              " password=" + m_password;
        
        m_conn = PQconnectdb(connStr.c_str());

        if (PQstatus(m_conn) != CONNECTION_OK) {
            std::string errorMsg = PQerrorMessage(m_conn);

            PQfinish(m_conn);
            m_conn = nullptr;
            m_isConnected = false;

            throw std::runtime_error("PostgreSQL connection failed: " + errorMsg);
        } 
        m_isConnected = true;

        const char* schemaSql = 
            "CREATE TABLE IF NOT EXISTS trades ("
            "   id SERIAL PRIMARY KEY,"
            "   symbol TEXT NOT NULL,"
            "   price NUMERIC NOT NULL,"
            "   quantity NUMERIC NOT NULL,"
            "   is_sell BOOLEAN NOT NULL,"
            "   timestamp BIGINT NOT NULL"
            ");";


        PGresult* res = PQexec(m_conn, schemaSql);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::string error = PQresultErrorMessage(res);
            PQclear(res);
            PQfinish(m_conn);
            m_conn = nullptr;
            m_isConnected = false;
            throw std::runtime_error("Failed to create table: " + error);
        }
        PQclear(res);
    }

MarketDataStore::~MarketDataStore() {
    if (m_conn) {
        PQfinish(m_conn);
    }
}

void MarketDataStore::updateDB(Trade t) {
    if (!m_isConnected) {
        throw std::runtime_error("Cannot update: Not connected to the database");
    }

    const char* sql = "INSERT INTO trades (symbol, price, quantity, is_sell, timestamp) VALUES ($1, $2, $3, $4, $5)";

    std::string priceStr = std::to_string(t.price);
    std::string qtyStr = std::to_string(t.quantity);
    std::string isSellStr = t.is_sell ? "TRUE" : "FALSE";
    std::string tsStr = std::to_string(t.timestamp);

    const char* paramValues[5];
    paramValues[0] = t.symbol.c_str();
    paramValues[1] = priceStr.c_str();
    paramValues[2] = qtyStr.c_str();
    paramValues[3] = isSellStr.c_str();
    paramValues[4] = tsStr.c_str();

    PGresult* res = PQexecParams(
        m_conn,
        sql,
        5,
        nullptr,
        paramValues,
        nullptr,
        nullptr,
        0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string error = PQresultErrorMessage(res);
        PQclear(res);
        throw std::runtime_error("Insert failed: " + error);
    }
    PQclear(res);
}